#ifndef KERNEL_BUDDY_H_
#define KERNEL_BUDDY_H_

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <util.h>

typedef uint16_t bitmap_blk_t;
typedef uint16_t bitmap_count_t;

#define BUDDIES_SIZE(orders)  ((sizeof(bitmap_blk_t) << (orders+1)) - 1)
#define MAX_ORDERS            17

struct buddy_allocator {
  /**
      An array of pointers each pointing to a free block array of size 2^N,
      where N is the block order.
  */
  bitmap_blk_t *free_blocks[MAX_ORDERS];

  /// The number of block sizes, the smallest order (order 0) of size MIN_ORDER_SIZE.
  unsigned char orders;

  /**
      The base 2 logarithm of the minimum order size.
      i.e. min_order_size = 2^(log2_min_order_size)
  */

  unsigned char log2_min_order_size;

  /**
      The array of free block counts, one count for each order indicating
      the number of free blocks that are available in the corresponding
      free block array.
  */
  bitmap_count_t free_counts[MAX_ORDERS];
};

struct memory_block {
  void *ptr;
  size_t size;
};

/**
  Initialize a buddy allocator by placing it in a particular memory of region.

  @param allocator The allocator.
  @param mem_size The total amount of memory the allocator will manage in bytes.
  @param min_order_size The size of the smallest block in bytes
  (it will be rounded up to the nearest power of two).
  @param mem_region A pointer to an available region of memory that is large
  enough to store the free block arrays for each order, pointers to each
  free block array, and free block counts for each order. The BUDDIES_SIZE()
  macro can calculate this.
  @return E_OK, on success. E_INVALID_ARG, if mem_size would require the
  allocator to manage too many block orders.
*/

NON_NULL_PARAMS int buddy_init(struct buddy_allocator * restrict allocator,
  size_t mem_size, size_t min_order_size, void * restrict mem_region);

/**
  Calculate the smallest block order needed to satisfy a memory request.

  @param allocator The allocator.
  @param mem_size The size of the memory request in bytes. It must not be
  larger than the size of the highest order block.
  @return The order corresponding to a block of appropriate size.
*/

NON_NULL_PARAMS PURE size_t buddy_block_order(const struct buddy_allocator *allocator,
  size_t mem_size);

/**
  Allocate a block of memory.

  @param allocator The allocator.
  @param size The size of the memory request in bytes. It must not be
  larger than the size of the highest order block.
  @param block The structure where the block address and block size are
  set if sucessful.
  @return E_OK, on success. E_FAIL, on failure. E_OOM, if memory is
  completely exhausted. E_SIZE, if memory is available, but not of the requested
  block size.
*/

NON_NULL_PARAMS int buddy_alloc_block(struct buddy_allocator * restrict allocator, size_t size,
  struct memory_block * restrict block);

/**
  Release a block of memory.

  @param allocator The allocator.
  @param block The memory block that was set by buddy_alloc_block().
  @return E_OK, on success. E_INVALID_ARG, if the memory block is invalid.
*/

NON_NULL_PARAMS int buddy_free_block(struct buddy_allocator * restrict allocator,
  const struct memory_block * restrict block);

/**
  Concatenate smaller free blocks into larger ones by joining them with their
  buddies.

  @param allocator The allocator.
  @return true, if at least one larger block was formed as a result. false,
  if no blocks could be joined.
*/

NON_NULL_PARAMS bool buddy_coalesce_blocks(struct buddy_allocator *allocator);

/**
  Returns the amount of free, unallocated memory.

  @param allocator The allocator.
  @return The amount of free memory in bytes.
*/

NON_NULL_PARAMS PURE size_t buddy_free_bytes(const struct buddy_allocator *allocator);

NON_NULL_PARAMS PURE static inline size_t buddy_free_count(const struct buddy_allocator *allocator,
  unsigned int order) {
  return order >= allocator->orders ? 0 : (size_t)allocator->free_counts[order];
}

#endif /* KERNEL_BUDDY_H_ */
