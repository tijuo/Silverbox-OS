#ifndef MEM_REGION_H
#define MEM_REGION_H

#include <stddef.h>
#include <stdbool.h>

struct MemRegion
{
  unsigned int start;
  size_t length;
};

int reg_join( struct MemRegion *region1, struct MemRegion *region2 );
void reg_swap( struct MemRegion *region1, struct MemRegion *region2 );
bool is_in_region( unsigned int addr, struct MemRegion *region );
bool reg_overlaps( struct MemRegion *region1, struct MemRegion *region2 );
bool reg_can_join( struct MemRegion *region1, struct MemRegion *region2 );
bool reg_is_adjacent( struct MemRegion *region1, struct MemRegion *region2 );

#endif /* MEM_REGION_H */
