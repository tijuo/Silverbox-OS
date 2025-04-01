#include "list.h"
#include "phys_alloc.h"
#include "shmem.h"
#include <stdlib.h>
#include <oslib.h>

/* Complete */

extern void *list_malloc( size_t );
extern void list_free(void * );
extern int _mapMem( void *phys, void *virt, int pages, int flags, void *pdir );

int init_shmem( shmid_t shmid, tid_t owner, unsigned pages, bool ro_perm )
{
  struct SharedMemory *new_shmem;
  unsigned phys_addr, addr;
  int i;

  if( owner == NULL_TID || owner == KERNEL_TID )
    return -1;

  if( pages > total_pages )
    return -1;

/*  if( list_lookup(shmid, &shmem_list) == true )
    return -1;*/

  new_shmem = malloc(sizeof(struct SharedMemory));

  if( new_shmem == NULL )
    return -1;

  new_shmem->owner = owner;
  new_shmem->shmid = shmid;
  new_shmem->ro_perm = ro_perm;

  new_shmem->phys_pages = malloc( pages * sizeof( unsigned long ) );

  if( new_shmem->phys_pages == NULL )
  {
    free(new_shmem);
    return -1;
  }

//  list_init(&new_shmem->shmem_region_list, list_malloc, list_free);

  for(i=0; i < pages; i++)
  {
    phys_addr = (unsigned int)alloc_phys_page( NORMAL, (void *)shmid );

    if( (unsigned)phys_addr == (unsigned)NULL_PADDR )
    {
      while( i-- > 0 )
      {
        addr = new_shmem->phys_pages[i];
        free_phys_page((void *)addr);
      }
      free(new_shmem->phys_pages);
      free(new_shmem);
      return -1;
    }

    new_shmem->phys_pages[i] = phys_addr;
    phys_page_list[(unsigned int)phys_addr / PAGE_SIZE].shared = 1;
    phys_page_list[(unsigned int)phys_addr / PAGE_SIZE].pdir_page = shmid;
  }

//  list_insert(shmid, new_shmem, &shmem_list);

  return 0;
}

/* shmem_attach() and _shmem_attach() don't do anything with access
   permissions. */

int _shmem_attach( struct SharedMemory *shmem, struct AddrSpace \
                  *addr_space, MemRegion *region)
{
  struct ShMemRegion *shmem_region;
  unsigned int addr, i;

  if( shmem == NULL || addr_space == NULL || region == NULL )
    return -1;

  if( addr_space_region_intersects( addr_space, region ) )
    return -1;

  shmem_region = malloc( sizeof( struct ShMemRegion ) );

  if( shmem_region == NULL )
     return -1;

  shmem_region->region = *region;
  shmem_region->addr_space = addr_space;
  shmem_region->rw = shmem->ro_perm;

  for( addr = region->start, i=0; addr < region->start + region->length; \
       addr += PAGE_SIZE, i++ )
  {
    _mapMem( (void *)shmem->phys_pages[i], (void *)addr, 1, 0,
             addr_space->phys_addr );
  }
  list_insert((int)shmem_region, shmem_region, &shmem->shmem_region_list);
  return 0;
}

int shmem_attach( shmid_t shmid, struct AddrSpace *addr_space, \
                 MemRegion *region )
{
  struct SharedMemory *shmem;

  list_get_element((int)shmid, (void **)&shmem, &shmem_list);
  return _shmem_attach( shmem, addr_space, region );
}

int _shmem_detach( struct SharedMemory *sh_region, MemRegion *region )
{
  return -1;
}

int shmem_detach( shmid_t shmid, MemRegion *region )
{
  return -1;
}
