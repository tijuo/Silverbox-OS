#include <x86intrin.h>
#include <kernel/buddy.h>
#include <kernel/error.h>
#include <kernel/debug.h>
#include <stdio.h>
#include <string.h>
#include <kernel/pae.h>
#include <kernel/algo.h>
#include <kernel/memory.h>

#define MIN_ORDER_SIZE(allocator)   (1u << (allocator)->log2_min_order_size)

/**
  Comparator for the kqsort() function.

  @param b1 Free block 1
  @param b2 Free block 2
  @return -1 if block 1 goes before block 2. 1 if block 1 goes after block 2.
  0 if block 1 and block 2 refer to the same block.
 */
NON_NULL_PARAMS PURE static int buddy_block_cmp(const void *b1, const void *b2);

int buddy_init(struct buddy_allocator *allocator, size_t mem_size,
               size_t min_order_size, void *mem_region)
{
  allocator->log2_min_order_size = (unsigned char)__bsrq(min_order_size);

  if(__bsfq(min_order_size) != (int)allocator->log2_min_order_size)
    allocator->log2_min_order_size++;

  unsigned int orders = (unsigned int)__bsrq(mem_size);

  if(orders != (unsigned int)__bsfq(mem_size))
    orders++;

  orders -= allocator->log2_min_order_size - 1;

  if(orders > MAX_ORDERS)
    RET_MSG(E_INVALID_ARG,
            "Number of block orders must be at least 0 and no more than MAX_ORDERS");

  allocator->orders = orders;

  for(size_t i = 0, offset = 0; i < MAX_ORDERS; i++, offset += (i <= orders ? (1ul << (orders-i)) : 0)) {
    if(i < orders)
      allocator->free_blocks[i] = (bitmap_blk_t *)mem_region + offset;
    else
      allocator->free_blocks[i] = NULL;
  }

  kmemset(allocator->free_counts, 0, sizeof allocator->free_counts);

  allocator->free_blocks[orders - 1][0] = 0;
  allocator->free_counts[orders - 1] = 1;

  return E_OK;
}

size_t buddy_block_order(const struct buddy_allocator *allocator,
                         size_t mem_size)
{
  if(mem_size <= MIN_ORDER_SIZE(allocator))
    return 0;
  else if(mem_size >= (MIN_ORDER_SIZE(allocator) << (allocator->orders - 1)))
    return allocator->orders - 1;
  else {
    unsigned int msb_index = (unsigned int)__bsrq(mem_size);

    if(msb_index != (unsigned int)__bsfq(mem_size))
      msb_index++;

    return msb_index - allocator->log2_min_order_size;
  }
}

int buddy_alloc_block(struct buddy_allocator *allocator, size_t size,
                      struct memory_block *block)
{
  unsigned int block_order = buddy_block_order(allocator, size);

  // Look for the smallest free block that is at least as large as the requested
  // allocation size. If found, remove it from the free array and return it. Otherwise,
  // repeatedly split blocks in half until a block of the desired size is obtained.

  for(unsigned int order = block_order; order < allocator->orders; order++) {
    if(allocator->free_counts[order] > 0) {
      bitmap_blk_t free_block =
        allocator->free_blocks[order][allocator->free_counts[order] - 1];

      // Split a large block into two halves, then split a half into two quarters,
      // split a quarter into two eights, and so on until a block of the desired
      // order remains.

      while(order > block_order) {
        allocator->free_counts[order] -= 1;

        bitmap_count_t next_size = allocator->free_counts[order - 1];
        bitmap_blk_t *next_array = allocator->free_blocks[order - 1];

        next_array[next_size] = 2 * free_block + 1;
        next_array[next_size + 1] = 2 * free_block;

        allocator->free_counts[order - 1] += 2;
        free_block *= 2;
        order -= 1;
      }

      allocator->free_counts[order] -= 1;

      block->size = MIN_ORDER_SIZE(allocator) << block_order;
      block->ptr = (void *)(allocator->free_blocks[order][allocator->free_counts[order]]
                            * block->size);

      return E_OK;
    }
  }

  // If no free blocks were found, then coalesce smaller blocks into larger ones
  // and try to allocate memory again

  buddy_coalesce_blocks(allocator);

  if(allocator->free_counts[block_order] != 0)
    return buddy_alloc_block(allocator, size, block);
  else {
    for(size_t i = 0; i < allocator->orders; i++) {
      if(allocator->free_counts[i] > 0)
        RET_MSG(E_SIZE, "Requested block is too large.");
    }
  }

  RET_MSG(E_OOM, "Out of memory.");
}

int buddy_reserve_block(struct buddy_allocator *allocator, const void *addr, size_t size,
                        struct memory_block *mem_block)
{
  size_t block_order = buddy_block_order(allocator, size);
  bitmap_blk_t block_num = (bitmap_blk_t)((uintptr_t)addr >> (allocator->log2_min_order_size + block_order));

  /* Look to see if the block has been marked as free. If so, then
     simply remove it from the free block array. Otherwise, find the
     the smallest superblock that it belongs to and then continually split
     it until we get the reserved block. Finally, remove the reserved block.
  */

  for(unsigned int order = block_order; order < allocator->orders; order++) {
    for(size_t i = 0; i < allocator->free_counts[order]; i++) {
      if(allocator->free_blocks[order][i] == (block_num >> (order - block_order))) {
        // A free block was found. Remove it from the free block array.

        for(; i + 1 < allocator->free_counts[order]; i++)
          allocator->free_blocks[order][i] = allocator->free_blocks[order][i + 1];

        allocator->free_counts[order]--;

        /* If the found block was a super-block, then split the block into two
           sub-blocks: one to which the reserved block belongs and its buddy. Mark the
           buddy as free. Keep doing this until we're left with a sub-block
           that's the reserved block. */

        while(order-- > block_order) {
          allocator->free_blocks[order][allocator->free_counts[order]] =
            (block_num >> order) ^ 1u;
          allocator->free_counts[order]++;
        }

        if(mem_block) {
          mem_block->size = (1ul << (allocator->log2_min_order_size + block_order));
          mem_block->ptr = (void *)(block_num * mem_block->size);
        }

        return E_OK;
      }
    }
  }

  // The reserved block couldn't be found (because it was already allocated or reserved)

  return E_FAIL;
}

int buddy_free_block(struct buddy_allocator *allocator,
                     const struct memory_block *block)
{
  if(block->size
      > MIN_ORDER_SIZE(allocator) << (allocator->orders - 1)
      || block->size < MIN_ORDER_SIZE(allocator)) {
    RET_MSG(E_INVALID_ARG, "Invalid block size.");
  }

  unsigned int block_order = buddy_block_order(allocator, block->size);
  bitmap_blk_t block_number = (bitmap_blk_t)((uintptr_t)block->ptr
                              / (uintptr_t)(1ul << block_order) * MIN_ORDER_SIZE(allocator));

  if(block_number >= (1u << (allocator->orders - 1 - block_order)))
    RET_MSG(E_INVALID_ARG, "Invalid block.");

  allocator->free_blocks[block_order][allocator->free_counts[block_order]++] =
    block_number;

  buddy_coalesce_blocks(allocator);

  return E_OK;
}

int buddy_block_cmp(const void *b1, const void *b2)
{
  const bitmap_blk_t *bl1 = (const bitmap_blk_t *)b1;
  const bitmap_blk_t *bl2 = (const bitmap_blk_t *)b2;

  return *bl1 < *bl2 ? -1 : !!(*bl1 - *bl2);
}

bool buddy_coalesce_blocks(struct buddy_allocator *allocator)
{
  bool did_coalesce = false;

  for(unsigned int order = 0; order < allocator->orders; order++) {
    bitmap_blk_t shift = 0;
    bitmap_blk_t free_blocks = allocator->free_counts[order];

    kqsort(allocator->free_blocks[order], (size_t)free_blocks, sizeof(bitmap_blk_t),
           buddy_block_cmp);

    for(bitmap_blk_t i = 0; i < free_blocks;) {
      bitmap_blk_t coalesced_blk;
      bitmap_blk_t *array = allocator->free_blocks[order];

      if(i + 1 < free_blocks && array[i] + 1 == array[i + 1]
          && array[i] % 2 == 0) {
        did_coalesce = true;
        coalesced_blk = array[i] / 2;
      } else {
        array[i - shift] = array[i];
        i += 1;
        continue;
      }

      bitmap_blk_t larger_count = allocator->free_counts[order + 1];

      allocator->free_blocks[order + 1][larger_count] = coalesced_blk;
      allocator->free_counts[order + 1] += 1;
      shift += 2;
      i += 2;
    }

    allocator->free_counts[order] -= shift;
  }

  return did_coalesce;
}

size_t buddy_free_bytes(const struct buddy_allocator *allocator)
{
  size_t bytes = 0;

  for(size_t i = 0; i < allocator->orders; i++)
    bytes += allocator->free_counts[i] * (1ul << i) * MIN_ORDER_SIZE(allocator);

  return bytes;
}
