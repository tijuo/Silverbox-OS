#include <os/region.h>

bool regionCanJoin( const struct MemRegion *region1, const struct MemRegion *region2 )
{
  return regionDoesOverlap( region1, region2 ) || regionIsAdjacent( region1, region2 );
}

/* Concatenates region 2 to region 1 storing the result in region 1. */

int regionJoin( struct MemRegion *region1, const struct MemRegion *region2 )
{
  addr_t newStart;
  size_t newLength;

  if ( !regionCanJoin( region1, region2 ) )
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

bool regionDoesContain( addr_t addr, const struct MemRegion *region )
{
  return addr >= region->start && addr < region->start + region->length;
}

bool regionDoesOverlap( const struct MemRegion *region1, const struct MemRegion *region2 )
{
  return regionDoesContain( region2->start, region1 ) || regionDoesContain( region1->start, region2 );
}

/* Indicates whether one region ends exactly where the other region begins. */

bool regionIsAdjacent( const struct MemRegion *region1, const struct MemRegion *region2 )
{
  return region1->start + region1->length == region2->start
         || region2->start + region2->length == region1->start;
}

void regionSwap( struct MemRegion *region1, struct MemRegion *region2 )
{
  addr_t tStart;
  size_t tLen;

  tStart = region2->start;
  tLen = region2->length;

  region2->start = region1->start;
  region2->length = region1->length;

  region1->start = tStart;
  region1->length = tLen;
}

