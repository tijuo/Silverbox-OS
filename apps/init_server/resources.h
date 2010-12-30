#ifndef RESOURCES_H
#define RESOURCES_H

#include "addr_space.h"
#include <os/os_types.h>

struct ResourcePool
{
  rspid_t id;
  struct AddrSpace addrSpace;

  struct IOPermBitmaps
  {
    char *phys1;
    char *phys2;
  } ioBitmaps;

  struct Executable *execInfo;
  SBArray tids;
};

SBAssocArray tidTable; // tid -> struct ResourcePool *
SBAssocArray physAspaceTable; // phys addr -> struct ResourcePool *
SBAssocArray resourcePools; // rspid -> struct ResourcePool *

int __create_resource_pool(struct ResourcePool *pool, void *phys_aspace);
struct ResourcePool *_create_resource_pool(void *phys_aspace);
struct ResourcePool *create_resource_pool(void);
int destroy_resource_pool(struct ResourcePool *);

int attach_resource_pool(struct ResourcePool *pool);
struct ResourcePool *detach_resource_pool(rspid_t id);
struct ResourcePool *lookup_rspid(rspid_t id);

int attach_tid(struct ResourcePool *pool, tid_t tid);
struct ResourcePool *detach_tid(tid_t tid);
struct ResourcePool *lookup_tid(tid_t tid);

int attach_phys_aspace(struct ResourcePool *pool, void *addr);
struct ResourcePool *detach_phys_aspace(void *addr);
struct ResourcePool *lookup_phys_aspace(void *addr);

#endif /* RESOURCES_H */
