#ifndef MEM_REGION_H
#define MEM_REGION_H

#include <stddef.h>
#include <stdbool.h>
#include <types.h>

struct MemRegion
{
  addr_t start;
  size_t length;
};

int  regionJoin( struct MemRegion *region1, const struct MemRegion *region2 );
void regionSwap( struct MemRegion *region1, struct MemRegion *region2 );
bool regionDoesContain( addr_t addr, const struct MemRegion *region );
bool regionDoesOverlap( const struct MemRegion *region1, const struct MemRegion *region2 );
bool regionCanJoin( const struct MemRegion *region1, const struct MemRegion *region2 );
bool regionIsAdjacent( const struct MemRegion *region1, const struct MemRegion *region2 );

#endif /* MEM_REGION_H */
