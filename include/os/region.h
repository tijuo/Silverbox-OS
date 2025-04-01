#ifndef MEM_REGION_H
#define MEM_REGION_H

#include <stddef.h>
#include <stdbool.h>
#include <types.h>

typedef struct {
    addr_t start;
    addr_t end;
} MemRegion;

/**
 * Initialize a memory region. Memory addresses within the half-open interval specified by
 * [start, end) are within the region.
 * 
 * The start address must not equal the end address.
 * 
 * @param region The memory region to be initialized.
 * @param start The region's start address.
 * @param end The region's end address. It must be greater than the start address.
 * @return `0`, on success. `-1`, if the end address is less than or equal to the start address.
 */
static inline int region_init(MemRegion *region, addr_t start, addr_t end) {
    if(start < end) {
        region->start = start;
        region->end = end;

        return 0;
    }

    return -1;
}

/** Return the length of a memory region.
 * 
 * @param region A memory region.
 * @return The region's length, in bytes.
*/
static inline size_t region_length(const MemRegion *region) {
    return region->end - region->start;
}

static inline int region_difference(const MemRegion *minuend, const MemRegion *subtrahend, MemRegion (*differences)[2]) {
    int count = 0;

    if(minuend->start < subtrahend->start) {
        differences[count]->start = minuend->start;
        differences[count]->end = subtrahend->start;
        count += 1;
    }

    if(minuend->end > subtrahend->end) {
        differences[count]->start = subtrahend->end;
        differences[count]->end = minuend->end;
        count += 1;
    }

    return count;
}

static inline bool region_equals(const MemRegion *region, const MemRegion *other) {
    return region->start == other->start && region->end == other->end;
}

/**
 * Determine whether an address lies within a memory region.
 * 
 * @param region A memory region.
 * @param addr The address.
 * @return `true`, if the address is within the region. `false`, otherwise.
 */
static inline bool region_contains(const MemRegion* region, addr_t addr)
{
    return addr >= region->start && addr < region->end;
}

/** Indicate whether one region ends exactly where the other region begins.
 * 
 * @param region A memory region.
 * @param other Another memory region.
 * @return `true`, if the two regions are adjacent. `false` otherwise.
*/
static inline bool region_is_adjacent(const MemRegion* region, const MemRegion* other)
{
    return region->end == other->start || other->end == region->start;
}

/**
 * Determine whether two regions intersect (i.e. addresses of one region are contained in the other).
 * 
 * @param region A memory region.
 * @param other Another memory region.
 * @return `true`, if addresses in one region are contained in the other. `false`, if the two regions
 * are disjoint.
 */
static inline bool region_intersects(const MemRegion* region, const MemRegion* other)
{
    return region_contains(region, other->start) || region_contains(other, region->start);
}

/**
 * Determine whether two memory regions can be joined.
 *
 * @param region A memory region.
 * @param other Another memory region.
 * @return `true`, if the two regions can be joined. `false`, otherwise.
 */
static inline bool region_can_concatenate(const MemRegion* region, const MemRegion* other)
{
    return region_intersects(region, other) || region_is_adjacent(region, other);
}

/** Concatenate two regions together.
 * 
 * @param augend The memory region with which an addend region with be concatenated. `augend` will then contain the result.
 * @param addend The memory region to be concatenated.
 * @return `true`, on success. `false`, on failure.
 */
static inline bool region_concatenate(MemRegion* augend, const MemRegion* addend)
{
    if(!region_can_concatenate(augend, addend)) {
        return false;
    } else {
        *augend = (MemRegion){
            .start = augend->start <= addend->start ? augend->start : addend->start,
            .end = augend->end >= addend->end ? augend->end : addend->end
        };

        return true;
    }
}

#endif /* MEM_REGION_H */
