#ifndef _SHMEM_H
#define _SHMEM_H

#include "addr_space.h"
#include <stdbool.h>
#include <os/region.h>
#include <oslib.h>

struct SharedMemory
{
  tid_t owner;
  shmid_t shmid;
  bool ro_perm;
  unsigned long *phys_pages;
  DynArray memory_regions;
//  struct ListType shmem_region_list;
};

struct ShMemRegion
{
  MemRegion region;
  struct AddrSpace *addr_space;
  bool rw;
};

//struct ListType shmem_list;

int init_shmem( shmid_t shmid, tid_t owner, unsigned pages, bool ro_perm );
int _shmem_attach( struct SharedMemory *shmem, struct AddrSpace \
                  *addr_space, MemRegion *region);
int shmem_attach( shmid_t shmid, struct AddrSpace *addr_space, \
                 MemRegion *region );
int _shmem_detach( struct SharedMemory *sh_region, MemRegion *region );
int shmem_detach( shmid_t shmid, MemRegion *region );

#endif /* _SHMEM_H */
