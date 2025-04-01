#ifndef PAGER_H
#define PAGER_H

#include "addr_space.h"

int region_map(AddrSpace *addr_space, addr_t virt, paddr_t phys, size_t pages, int flags);

#endif /* PAGER_H */
