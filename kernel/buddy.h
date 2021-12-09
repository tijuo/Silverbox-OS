#ifndef KERNEL_BUDDY_H_
#define KERNEL_BUDDY_H_

#include <stdbool.h>
#include <assert.h>
#include <stdint.h>
#include <stddef.h>

#define E_OK 0
#define E_FAIL -1
#define E_INVALID_ARG -2
#define E_SIZE -10
#define E_OOM -11

#define RET_MSG(v, msg) { printf("%s\n", msg); return v; }
#define NON_NULL_PARAMS __attribute__((nonnull))

#define BUDDIES_SIZE(orders) (sizeof(uint32_t) * (((1 << (orders+1)) - 1) + orders) \
	+ orders * sizeof(uint32_t*))

#define MIN_ORDER_BITS		16
#define MIN_ORDER_SIZE		(1 << MIN_ORDER_BITS)

static_assert(MIN_ORDER_SIZE >= 4096, "Minimum order size must be at least 4 KiB.");

struct buddies {
  // An array of pointers each pointing to a free block array of size 2^N,
  // where N is the block order.
  uint32_t **free_blocks;

  // The array of free block counts, one count for each order indicating
  // the number of free blocks that are available in the corresponding
  // free block array.
  uint32_t *free_counts;

  // The number of block sizes, the smallest order (order 0) of size MIN_ORDER_SIZE.
  unsigned int orders;
};

struct memory_block {
  void *ptr;
  size_t size;
};

NON_NULL_PARAMS int buddies_init(struct buddies *restrict buddies,
								 size_t mem_size, void *restrict mem_region);
NON_NULL_PARAMS unsigned int buddies_block_order(struct buddies *buddies,
												 size_t mem_size);
NON_NULL_PARAMS int buddies_alloc_block(struct buddies *buddies, size_t size,
										struct memory_block *block);
NON_NULL_PARAMS int buddies_free_block(struct buddies *buddies,
									   struct memory_block *block);
NON_NULL_PARAMS bool buddies_coalesce_blocks(struct buddies *buddies);
NON_NULL_PARAMS int buddies_block_cmp(const void *b1, const void *b2);
NON_NULL_PARAMS size_t buddies_free_bytes(struct buddies *buddies);

#endif /* KERNEL_BUDDY_H_ */
