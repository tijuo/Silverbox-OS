#ifndef PAGER_H
#define PAGER_H

#include "addr_space.h"

int mapRegion(struct AddrSpace *addrSpace, addr_t virt, paddr_t phys, size_t pages, int flags);

#endif /* PAGER_H */
