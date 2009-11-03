#include <os/region.h>

bool reg_can_join( struct MemRegion *region1, struct MemRegion *region2 )
{
  if ( reg_overlaps( region1, region2 ) || reg_is_adjacent( region1, region2 ) )
    return true;
  else
    return false;
}

/* Joins region2 to region1 */

int reg_join( struct MemRegion *region1, struct MemRegion *region2 )
{
  unsigned int newStart;
  size_t newLength;

  if ( !reg_can_join( region1, region2 ) )
    return -1;

  if ( region1->start <= region2->start )
  {
    newStart = region1->start;

    if ( region1->start + region1->length >= region2->start + region2->length )
      newLength = region1->length;
    else
      newLength = ( region2->start + region2->length ) - newStart;
  }
  else
  {
    newStart = region2->start;

    if ( region2->start  + region2->length >= region1->start + region1->length )
      newLength = region2->length;
    else
      newLength = ( region1->start + region1->length ) - newStart;
  }

  region1->start = newStart;
  region1->length = newLength;

  return 0;
}

bool is_in_region( unsigned int addr, struct MemRegion *region )
{
  if ( addr >= region->start && addr < region->start + region->length )
      return true;
  else
    return false;
}

bool reg_overlaps( struct MemRegion *region1, struct MemRegion *region2 )
{
  if ( is_in_region( region2->start, region1 ) || is_in_region( region1->start, region2 ) )
    return true;
  else
    return false;
}

bool reg_is_adjacent( struct MemRegion *region1, struct MemRegion *region2 )
{
  if ( region1->start + region1->length == region2->start )
    return true;
  else if ( region2->start + region2->length == region1->start )
    return true;
  else
    return false;
}

void reg_swap( struct MemRegion *region1, struct MemRegion *region2 )
{
  unsigned int tStart;
  size_t tLen;

  tStart = region2->start;
  tLen = region2->length;

  region2->start = region1->start;
  region2->length = region1->length;

  region1->start = tStart;
  region1->length = tLen;
}

