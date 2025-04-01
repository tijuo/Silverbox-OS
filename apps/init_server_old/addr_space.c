#include "addr_space.h"
#include "phys_alloc.h"
#include "initsrv.h"

/* There needs to be a way to lookup the address space of a thread using a TID */

/* Initializes an address space and its tables. The physical
   address is the address of the page directory. */

void addr_space_init(struct AddrSpace *addr_space, addr_t phys_addr)
{
  struct AddrRegion region1 = { { 0, 0xC0000 }, { 0, 0xC0000 }, REG_RESD };
  struct AddrRegion region2 = { { 0xFF400000, 0xC00000 }, { 0, 0 }, REG_RESD };

  if( addr_space == NULL )
    return;

  createBitmap(addr_space->bitarray, NUM_PTABLES);
  addr_space->phys_addr = phys_addr;
  sbArrayCreate(&addr_space->memory_regions);

  attach_mem_region(addr_space, &region1);
  attach_mem_region(addr_space, &region2);
  set_ptable_status(addr_space, (void *)0, true);
  set_ptable_status(addr_space, (void *)0x80000000, true);
  set_ptable_status(addr_space, (void *)0xFF800000, true);
  set_ptable_status(addr_space, (void *)0xFFC00000, true);
#if 0
  if( phys_addr != page_dir )
  {
    void *addr=alloc_phys_page(NORMAL, page_dir);
    clearPage(addr);
    __map_page_table( (void *)0x0, addr, 0, phys_addr );
  }
#endif /* 0 */
}

void delete_addr_space(struct AddrSpace *addr_space)
{
  if( !addr_space )
    return;

  sbArrayDelete(&addr_space->memory_regions);
}

/* _set_ptable_status() and set_ptable_status() are used to update the
   mapping status of a page table in its address space */
/*
int addr_space_add(struct AddrSpace *addr_space)
{
  if( addr_space == NULL )
    return -1;

  if( sbAssocArrayInsert(&addr_spaces, (void *)&addr_space->phys_addr, 
        sizeof addr_space->phys_addr, addr_space, sizeof *addr_space) != 0 )
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

  if( sbAssocArrayLookup(&addr_spaces, (void *)&phys_addr, sizeof(void *), 
        (void **)&addr_space, NULL) < 0 )
  {
    return NULL;
  }
  else
    return addr_space;
}

struct AddrSpace *remove_addr_space(void *phys_addr)
{
  struct AddrSpace *addr_space;

  if( phys_addr == NULL_PADDR )
    return NULL;

  if( sbAssocArrayRemove(&addr_spaces, (void *)&phys_addr, sizeof(void *),
        (void **)&addr_space, NULL) < 0 )
  {
    return NULL;
  }
  else
    return addr_space;
}
*/

int set_ptable_status(struct AddrSpace *addr_space, void *virt, bool status)
{
  if( addr_space == NULL )
    addr_space = &initsrv_pool;

  if( status == true )
    bitarray_set(addr_space->bitarray, (unsigned)virt / PTABLE_SIZE);
  else
    bitarray_clear(addr_space->bitarray, (unsigned)virt / PTABLE_SIZE);

  return 0;
}

/* Tells if the page table is mapped in its address space. */

bool get_ptable_status(struct AddrSpace *addr_space, void *virt)
{
  if( addr_space == NULL )
    addr_space = &initsrv_pool;

  return bitarray_is_set(addr_space->bitarray, (unsigned)virt / PTABLE_SIZE);
}

/* attach_tid() associates a TID to an address space. */
/*
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
*/

/* _lookup_tid() and lookup_tid() return the address space or physical address
   associated with a thread respectively. */

/*

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

*/

/* Virtual addresses are allocated to an address space by using
   _attach_mem_region() and attach_mem_region() */

int attach_mem_region(struct AddrSpace *addr_space, struct AddrRegion *region)
{
  struct AddrRegion *tRegion;

  if(addr_space == NULL)
    addr_space = &initsrv_pool;

  for(int i=0; i < sbArrayCount(&addr_space->memory_regions); i++)
  {
    if( sbArrayElemAt(&addr_space->memory_regions, i, (void **)&tRegion, NULL) != 0 )
      continue;

    if( reg_overlaps(&region->virt_region, &tRegion->virt_region) )
      return -1;
  }

  if( sbArrayPush(&addr_space->memory_regions, region, sizeof *region) == 0 )
    return 0;
  else
    return -1;
}

/* Returns true if a virtual address is allocated in an address space.
   Returns false otherwise. */

bool find_address(struct AddrSpace *addr_space, void *addr)
{
  struct AddrRegion *addr_region;

  if(addr_space == NULL)
    addr_space = &initsrv_pool;

  for( int i=0; i < sbArrayCount(&addr_space->memory_regions); i++ )
  {
    if( sbArrayElemAt(&addr_space->memory_regions, i, (void **)&addr_region, NULL) != 0 )
      return false;

    if( is_in_region((unsigned int)addr, &addr_region->virt_region) )
      return true;
  }

  return false;
}

/*
bool find_address(void *aspace_phys, void *addr)
{
  struct AddrSpace *addr_space;

  if( sbAssocArrayLookup(&addr_spaces, (void *)&aspace_phys, sizeof(void *), (void **)&addr_space,
    NULL) < 0 )
  {
    return false;
  }
  else
    return _find_address(addr_space, addr);
}
*/

/* Checks to see if there's a region that overlaps another region in an address space */

bool addr_space_region_intersects(struct AddrSpace *addr_space, MemRegion *region)
{
  struct AddrRegion *addr_region;

  if(addr_space == NULL)
    addr_space = &initsrv_pool;

  if( region == NULL )
    return false;

  for( int i=0; i < sbArrayCount(&addr_space->memory_regions); i++ )
  {
    if( sbArrayElemAt(&addr_space->memory_regions, i, (void **)&addr_region, NULL) != 0 )
      return false;

    if( reg_overlaps(region, &addr_region->virt_region) )
      return true;
  }

  return false;
}
/*
bool region_intersects(void *aspace_phys, MemRegion *region)
{
  struct AddrSpace *addr_space;

  if( sbAssocArrayLookup(&addr_spaces, (void *)&aspace_phys, sizeof(void *), (void **)&addr_space,
    NULL) < 0 )
  {
    return false;
  }
  else
    return addr_space_region_intersects(addr_space, region);
}
*/
