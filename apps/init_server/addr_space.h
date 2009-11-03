#ifndef ADDR_SPACE_H
#define ADDR_SPACE_H

#include <os/bitmap.h>
#include "list.h"
#include "phys_alloc.h"
#include <os/region.h>
#include <stddef.h>

#define NUM_PTABLES     1024

struct AddrSpace
{
  void *phys_addr;
  bitmap_t bitmap[NUM_PTABLES / 8];
  struct ListType mem_region_list, tid_list;
};

struct RegionNode
{
  struct MemRegion region;
  struct RegionNode *next, *prev;
};

struct ListNode *free_list_nodes;
struct ListType addr_space_list;

struct RegionNode *free_region_nodes;

void init_addr_space(struct AddrSpace *addr_space, void *phys_addr);
int set_ptable_status(void *aspace_phys, void *virt, bool status);
int _set_ptable_status(struct AddrSpace *addr_space, void *virt, bool status);
bool get_ptable_status(void *aspace_phys, void *virt);
bool _get_ptable_status(struct AddrSpace *addr_space, void *virt);
int attach_tid(void *aspace_phys, tid_t tid);
int _attach_tid(struct AddrSpace *addr_space, tid_t tid);
int detach_tid(void *aspace_phys, tid_t tid);
int _detach_tid(struct AddrSpace *addr_space, tid_t tid);
int attach_mem_region(void *aspace_phys, struct MemRegion *region);
int _attach_mem_region(struct AddrSpace *addr_space, struct MemRegion *region);
bool _region_overlaps(struct AddrSpace *addr_space, struct MemRegion *region);
bool region_overlaps(void *aspace_phys, struct MemRegion *region);
bool find_address(void *aspace_phys, void *addr);
bool _find_address(struct AddrSpace *addr_space, void *addr);
void *lookup_tid(tid_t tid);
struct AddrSpace *_lookup_tid(tid_t tid);
void region_free( void * );
void *region_malloc( void );
#endif /* ADDR_SPACE_H */
