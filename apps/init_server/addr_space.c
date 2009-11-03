#include "addr_space.h"
#include "pagersrv.h"

extern void mapPage(void);

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
    mapPage( );
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
  list_init(&addr_space->mem_region_list, list_malloc, list_free);
  list_init(&addr_space->tid_list, list_malloc, list_free);
}

void *region_malloc( void )
{
  struct RegionNode *node;
  size_t size = sizeof( struct RegionNode );

  if ( free_region_nodes != NULL )
  {
    node = free_region_nodes;
    free_region_nodes = free_region_nodes->next;

    if( free_region_nodes != NULL )
      free_region_nodes->prev = NULL;

    node->next = node->prev = NULL;

    return (void *)&node->region;
  }
  else if ( size > availBytes )
  {
    mapPage( );
    availBytes += PAGE_SIZE;
  }

  availBytes -= size;
  node = (struct RegionNode *)allocEnd;
  allocEnd = (void *)((size_t)allocEnd + size );

  return &node->region;
}

void region_free( void *n )
{
  struct RegionNode *node = (struct RegionNode *)n, *ptr = free_region_nodes;

  if ( node == NULL )
    return ;

  if ( free_region_nodes == NULL )
  {
    free_region_nodes = (struct RegionNode *)node;
    free_region_nodes->next = NULL;
    free_region_nodes->prev = NULL;
    return ;
  }

  while ( ptr->next != NULL )
    ptr = ptr->next;

  ptr->next = (struct RegionNode *)node;
  ptr->next->next = NULL;
  ptr->next->prev = ptr;
}

/* _set_ptable_status() and set_ptable_status() are used to update the
   mapping status of a page table in its address space */

int _set_ptable_status(struct AddrSpace *addr_space, void *virt, bool status)
{
  unsigned ptable_num = (unsigned)virt / PTABLE_SIZE;

  if( addr_space == NULL )
    return -1;

  if( status == true )
    setBitmapBit(addr_space->bitmap, ptable_num);
  else
    clearBitmapBit(addr_space->bitmap, ptable_num);

  return 0;
}

int set_ptable_status(void *aspace_phys, void *virt, bool status)
{
  struct AddrSpace addr_space, *ptr;
  ptr = &addr_space;

  if( list_get_element((int)aspace_phys, (void **)&ptr, &addr_space_list) != 0 )
    return -1;

  return _set_ptable_status(ptr, virt, status);
}

/* Tells if the page table is mapped in its address space. */

bool _get_ptable_status(struct AddrSpace *addr_space, void *virt)
{
  unsigned ptable_num = (unsigned)virt / PTABLE_SIZE;

  if( addr_space == NULL )
    return false;

  return bitIsSet(addr_space->bitmap, ptable_num);
}

bool get_ptable_status(void *aspace_phys, void *virt)
{
  struct AddrSpace addr_space, *ptr;
  ptr = &addr_space;

  if( list_get_element((int)aspace_phys, (void **)&ptr, &addr_space_list) != 0 )
    return false;

  return _get_ptable_status(ptr, virt);  
}

/* _attach_tid() and attach_tid() associates a TID to an address space. */

int _attach_tid(struct AddrSpace *addr_space, tid_t tid)
{
  return list_insert(tid, NULL, &addr_space->tid_list);
}

int attach_tid(void *aspace_phys, tid_t tid)
{
  struct AddrSpace addr_space, *ptr;
  ptr = &addr_space;

  if( list_get_element((int)aspace_phys, (void **)&ptr, &addr_space_list) != 0 )
    return -1;

  return _attach_tid(ptr, tid);
}

int _detach_tid(struct AddrSpace *addr_space, tid_t tid)
{
  return list_remove(tid, &addr_space->tid_list);
}

int detach_tid(void *aspace_phys, tid_t tid)
{
  struct AddrSpace addr_space, *ptr;
  ptr = &addr_space;

  if( list_get_element((int)aspace_phys, (void **)&ptr, &addr_space_list) != 0 )
    return -1;

  return _detach_tid(ptr, tid);
}

/* Virtual addresses are allocated to an address space by using
   _attach_mem_region() and attach_mem_region() */

int _attach_mem_region(struct AddrSpace *addr_space, struct MemRegion *region)
{
  return list_insert(region->start, region, &addr_space->mem_region_list);
}

int attach_mem_region(void *aspace_phys, struct MemRegion *region)
{
  struct AddrSpace addr_space, *ptr;
  ptr = &addr_space;

  if( region == NULL )
    return -1;

  if( list_get_element((int)aspace_phys, (void **)&ptr, &addr_space_list) != 0 )
    return -1;

  return _attach_mem_region(ptr, region);
}

/* Returns true if a virtual address is allocated in an address space.
   Returns false otherwise. */

bool _find_address(struct AddrSpace *addr_space, void *addr)
{
  struct ListNode *ptr = addr_space->mem_region_list.head;

  for(; ptr != NULL; ptr = ptr->next)
  {
    if( is_in_region((unsigned int)addr, (struct MemRegion *)ptr->element) )
      return true;
  }
  return false;
}

bool find_address(void *aspace_phys, void *addr)
{
  struct AddrSpace addr_space, *ptr;
  ptr = &addr_space;

  if( list_get_element((int)aspace_phys, (void **)&ptr, &addr_space_list) != 0 )
    return false;

  return _find_address(ptr, addr);
}

/* Checks to see if there's a region that overlaps another region in an address space */

bool _region_overlaps(struct AddrSpace *addr_space, struct MemRegion *region)
{
  struct ListNode *ptr = addr_space->mem_region_list.head;

  if( region == NULL )
    return false;

  for( ; ptr != NULL; ptr = ptr->next )
  {
    if( reg_overlaps(region, (struct MemRegion *)ptr->element ) )
      return true;
  }
  return false;
}

bool region_overlaps(void *aspace_phys, struct MemRegion *region)
{
  struct AddrSpace addr_space, *ptr;
  ptr = &addr_space;

  if( list_get_element((int)aspace_phys, (void **)&ptr, &addr_space_list) != 0 )
    return false;

  return _region_overlaps(ptr, region);
}

/* _lookup_tid() and lookup_tid() returns the address space or physical address 
   of the address space respectively. */

struct AddrSpace *_lookup_tid(tid_t tid)
{
  struct ListNode *ptr = addr_space_list.head;
  struct AddrSpace *addr_space;

  for(; ptr != NULL; ptr = ptr->next)
  {
    addr_space = (struct AddrSpace *)ptr->element;

    if( list_lookup(tid, &addr_space->tid_list ) == true )
      return addr_space;
  }

  return NULL;
}

void *lookup_tid(tid_t tid)
{
  struct AddrSpace *addr_space = _lookup_tid(tid);

  if( addr_space == NULL )
    return (void *)NULL_PADDR;

  return (void *)addr_space->phys_addr;
}
