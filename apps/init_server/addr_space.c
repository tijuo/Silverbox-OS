#include "addr_space.h"
#include "initsrv.h"

extern int extendHeap(unsigned);

/* There needs to be a way to lookup the address space of a thread using a TID */

void *list_malloc( size_t size )
{
  void *ptr;
  struct ListNode *node;

  if ( free_list_nodes != NULL )
  {
    node = free_list_nodes;
    free_list_nodes = free_list_nodes->next;

    if( free_list_nodes != NULL )
      free_list_nodes->prev = NULL;

    node->next = node->prev = NULL;

    return (void *)node;
  }

  else if ( size > availBytes )
  {
    extendHeap( 1 );
    availBytes += PAGE_SIZE;
  }

  availBytes -= size;
  ptr = allocEnd;
  allocEnd = (void *)((size_t)allocEnd + size );

  return ptr;
}

void list_free( void *node )
{
  struct ListNode *ptr = free_list_nodes;

  if ( node == NULL )
    return ;

  if ( free_list_nodes == NULL )
  {
    free_list_nodes = (struct ListNode *)node;
    free_list_nodes->next = NULL;
    free_list_nodes->prev = NULL;
    return ;
  }

  while ( ptr->next != NULL )
    ptr = ptr->next;

  ptr->next = (struct ListNode *)node;
  ptr->next->next = NULL;
  ptr->next->prev = ptr;
}

/* Initializes an address space and its tables. The physical
   address is the address of the page directory. */

void init_addr_space(struct AddrSpace *addr_space, void *phys_addr)
{
  if( addr_space == NULL )
    return;

  createBitmap(addr_space->bitmap, NUM_PTABLES);
  addr_space->phys_addr = phys_addr;
  sbArrayCreate(&addr_space->memoryRegions);
  list_init(&addr_space->mem_region_list, list_malloc, list_free);
}

/* _set_ptable_status() and set_ptable_status() are used to update the
   mapping status of a page table in its address space */

int addAddrSpace(struct AddrSpace *addrSpace)
{
  if( addrSpace == NULL )
    return -1;

  if( sbAssocArrayInsert(&addrSpaces, (void *)&addrSpace->phys_addr, 
        sizeof addrSpace->phys_addr, addrSpace, sizeof *addrSpace) != 0 )
  {
    return -1;
  }
  else
    return 0;
}

struct AddrSpace *lookupPhysAddr(void *phys_addr)
{
  struct AddrSpace *addr_space;

  if( phys_addr == NULL_PADDR )
    return NULL;

  if( (unsigned int *)phys_addr == page_dir )
    return &pager_addr_space;

  if( sbAssocArrayLookup(&addrSpaces, (void *)&phys_addr, sizeof(void *), 
        (void **)&addr_space, NULL) < 0 )
  {
    return NULL;
  }
  else
    return addr_space;
}

struct AddrSpace *removeAddrSpace(void *phys_addr)
{
  struct AddrSpace *addr_space;

  if( phys_addr == NULL_PADDR )
    return NULL;

  if( sbAssocArrayRemove(&addrSpaces, (void *)&phys_addr, sizeof(void *),
        (void **)&addr_space, NULL) < 0 )
  {
    return NULL;
  }
  else
    return addr_space;
}

int set_ptable_status(void *aspace_phys, void *virt, bool status)
{
  struct AddrSpace *addr_space = lookupPhysAddr(aspace_phys);

  if( addr_space == NULL )
    return -1;

  if( status == true )
    setBitmapBit(addr_space->bitmap, (unsigned)virt / PTABLE_SIZE);
  else
    clearBitmapBit(addr_space->bitmap, (unsigned)virt / PTABLE_SIZE);

  return 0;
}

/* Tells if the page table is mapped in its address space. */

bool get_ptable_status(void *aspace_phys, void *virt)
{
  struct AddrSpace *addr_space = lookupPhysAddr(aspace_phys);

  if( addr_space == NULL )
    return false;

  return bitIsSet(addr_space->bitmap, (unsigned)virt / PTABLE_SIZE);
}

/* attach_tid() associates a TID to an address space. */

int attach_tid(void *aspace_phys, tid_t tid)
{
  struct _Thread *thread;

  if( aspace_phys == NULL_PADDR || tid == NULL_TID )
    return -1;

  thread = malloc(sizeof(struct _Thread));

  if( !thread )
    return -1;

  thread->tid = tid;
  thread->aspace_phys = aspace_phys;

  if(sbAssocArrayInsert(&threadTable, &tid, sizeof tid, thread, sizeof *thread) != 0)
  {
    free(thread);
    return -1;
  }
  else
    return 0;
}

int detach_tid(tid_t tid)
{
  struct _Thread *thread;

  if( tid == NULL_TID )
    return -1;

  if( sbAssocArrayRemove(&threadTable, &tid, sizeof tid, (void **)&thread, 
      NULL) < 0 )
  {
    return -1;
  }
  else
  {
    free(thread);
    return 0;
  }
}

/* _lookup_tid() and lookup_tid() return the address space or physical address 
   associated with a thread respectively. */

struct AddrSpace *_lookup_tid(tid_t tid)
{
  return lookupPhysAddr(lookup_tid(tid));
}

void *lookup_tid(tid_t tid)
{
  struct _Thread *thread;

  if( tid == NULL_TID )
    return NULL_PADDR;

  if( sbAssocArrayLookup(&threadTable, &tid, sizeof tid, (void **)&thread, 
      NULL) < 0 )
  {
    return (void *)NULL_PADDR;
  }
  else
    return thread->aspace_phys;
}

/* Virtual addresses are allocated to an address space by using
   _attach_mem_region() and attach_mem_region() */

int _attach_mem_region(struct AddrSpace *addr_space, struct AddrRegion *region)
{
  struct AddrRegion *newRegion;

  if( !region )
    return -1;

  newRegion = malloc(sizeof(struct AddrRegion));

  if( !newRegion )
    return -1;

  *newRegion = *region;

  return list_insert(region->virtRegion.start, newRegion,
                     &addr_space->mem_region_list);
}

int attach_mem_region(void *aspace_phys, struct AddrRegion *region)
{
  struct AddrSpace *addr_space = lookupPhysAddr(aspace_phys);

  if( !addr_space )
    return -1;
  else
    return _attach_mem_region(addr_space, region);
}

/* Returns true if a virtual address is allocated in an address space.
   Returns false otherwise. */

bool _find_address(struct AddrSpace *addr_space, void *addr)
{
  struct ListNode *ptr = addr_space->mem_region_list.head;

  for(; ptr != NULL; ptr = ptr->next)
  {
    if( is_in_region((unsigned int)addr, 
        &((struct AddrRegion *)ptr->element)->virtRegion) )
    {
      return true;
    }
  }

  return false;
}

bool find_address(void *aspace_phys, void *addr)
{
  struct AddrSpace *addr_space;

  if( sbAssocArrayLookup(&addrSpaces, (void *)&aspace_phys, sizeof(void *), (void **)&addr_space,
    NULL) < 0 )
  {
    return false;
  }
  else
    return _find_address(addr_space, addr);
}

/* Checks to see if there's a region that overlaps another region in an address space */

bool _region_overlaps(struct AddrSpace *addr_space, struct MemRegion *region)
{
  struct ListNode *ptr = addr_space->mem_region_list.head;

  if( region == NULL )
    return false;

  for( ; ptr != NULL; ptr = ptr->next )
  {
    if( reg_overlaps(region, &((struct AddrRegion *)ptr->element)->virtRegion ) )
      return true;
  }
  return false;
}

bool region_overlaps(void *aspace_phys, struct MemRegion *region)
{
  struct AddrSpace *addr_space;

  if( sbAssocArrayLookup(&addrSpaces, (void *)&aspace_phys, sizeof(void *), (void **)&addr_space,
    NULL) < 0 )
  {
    return false;
  }
  else
    return _region_overlaps(addr_space, region);
}
