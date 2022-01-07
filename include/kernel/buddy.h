#ifndef KERNEL_BUDDY_H_
#define KERNEL_BUDDY_H_

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <util.h>
#include <types.h>
#include <kernel/types/bitmap.h>
#include <kernel/types/list.h>

typedef uint16_t bitmap_blk_t;
typedef uint16_t bitmap_count_t;

#define BUDDIES_SIZE(orders)  ((sizeof(bitmap_blk_t) << ((orders)+1)) - 1ul)
#define MAX_ORDERS            29

struct buddy_free_block_node {
  list_node_t node;
  unsigned long int order;
  unsigned long int checksum;
};

struct buddy_allocator {
  void *start_address;
  /**
    * A bit array in which each bit indicates that a valid free block struct
    * exists at the start of a block.
    */
  struct bitmap block_bitmap;

  /// An array of free block lists

  list_t free_block_lists[MAX_ORDERS];

  /// The number of block sizes, the smallest order (order 0) of size MIN_ORDER_SIZE.
  unsigned int orders;

  /**
    * The base 2 logarithm of the minimum order size.
    * i.e. min_order_size = 2^(log2_min_order_size)
    */

  unsigned int log2_min_order_size;

  lock_t lock;
};

struct memory_block {
  void *ptr;
  size_t size;
};

/**
  Initialize a buddy allocator that manages a contiguous range of free memory.

  @param allocator The allocator.
  @param mem_size The total amount of memory the allocator will manage in bytes.
  @param min_order_size The size of the smallest block in bytes
  (it will be rounded up to the nearest power of two).
  @param mem_region A pointer to an available region of memory that is large
  enough to store the block bitmap.
  @param start_address The beginning of the region of memory that will be managed.
  @return E_OK, on success. E_INVALID_ARG, if mem_size would require the
  allocator to manage too many block orders.
*/

NON_NULL_PARAM(1) NON_NULL_PARAM(4) int buddy_init(struct buddy_allocator *allocator,
                               size_t mem_size, size_t min_order_size, void *mem_region,
                               void *start_address);

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

NON_NULL_PARAMS int buddy_alloc_block(struct buddy_allocator *allocator, size_t size,
                                      struct memory_block *block);

/**
  Set a free block of memory as reserved.

  @param allocator The allocator.
  @param addr The start of the memory block. It should be a multiple of the minimum block size.
  @param size The size of the memory block.
  @param block The structure where the block address and block size are
  set if sucessful.
  @return E_OK, on success. E_FAIL, on failure (because the block isn't free).
*/

NON_NULL_PARAM(1) int buddy_reserve_block(struct buddy_allocator *allocator, const void *addr, size_t size,
                                        struct memory_block *mem_block);

/**
  Release a block of memory.

  @param allocator The allocator.
  @param block The memory block that was set by buddy_alloc_block().
  @return E_OK, on success. E_INVALID_ARG, if the memory block is invalid.
*/

NON_NULL_PARAMS int buddy_free_block(struct buddy_allocator *allocator,
                                     const struct memory_block *block);

/**
  Returns the amount of free, unallocated memory.

  @param allocator The allocator.
  @return The amount of free memory in bytes.
*/

NON_NULL_PARAMS size_t buddy_free_bytes(struct buddy_allocator *allocator);

#endif /* KERNEL_BUDDY_H_ */
