#ifndef RESPOOL_H
#define RESPOOL_H

#include "addr_space.h"
#include "list.h"

struct ResourcePool
{
  struct AddrSpace addrSpace;

  struct IOPermBitmaps
  {
    char *phys1;
    char *phys2;
  } ioBitmaps;

  struct Executable execInfo;
  struct ListType tid_list;
};

#endif /* RESPOOL_H */
