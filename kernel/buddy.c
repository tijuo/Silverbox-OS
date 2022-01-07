#include <x86intrin.h>
#include <kernel/buddy.h>
#include <kernel/error.h>
#include <kernel/debug.h>
#include <stdio.h>
#include <string.h>
#include <kernel/pae.h>
#include <kernel/algo.h>
#include <kernel/memory.h>
#include <kernel/sync.h>

#define MIN_ORDER_SIZE(allocator)   (1u << (allocator)->log2_min_order_size)

/** Calculate the correct checksum for a free memory block.
  *
  * @param block The free block node for which the checksum should be computed.
  * @return The checksum.
  */

NON_NULL_PARAMS PURE static inline unsigned long int
buddy_calc_checksum(const struct buddy_free_block_node *block) {
  return (unsigned long int)-((uintptr_t)block->node.item.item_void_ptr + (uintptr_t)block->node.next +
                              (uintptr_t)block->node.prev + block->order);
}

/** Determine whether a block's checksum is valid.
  *
  * @param block The free block to check.
  * @return true, if the checksum is valid. false, if the checksum isn't valid
  * (which indicates that the address doesn't point to a free block or memory was corrupted).
  */

NON_NULL_PARAMS PURE static inline bool
buddy_is_checksum_valid(const struct buddy_free_block_node *block) {
  return (uintptr_t)block->node.item.item_void_ptr + (uintptr_t)block->node.next +
         (uintptr_t)block->node.prev + block->order + block->checksum == 0;
}

/** Calculate the block bitmap offset of a memory block.
  *
  * @param allocator The buddy allocator.
  * @param addr The address of the block.
  * @return The block offset, n, where addr = n * order0_size
  */

NON_NULL_PARAMS PURE static inline unsigned long int
buddy_get_block_number(const struct buddy_allocator *allocator, void *addr) {
  return (unsigned long int)(((uintptr_t)addr - (uintptr_t)allocator->start_address) >> allocator->log2_min_order_size);
}

/** Mark a block as free.
  *
  * @param allocator The buddy allocator.
  * @param addr The address of the block to be marked as free.
  * @param order The size order of the block.
  */

NON_NULL_PARAM(1) static void buddy_add_free_block(struct buddy_allocator *allocator,
    void *addr, unsigned int order) {
  struct buddy_free_block_node *free_block = (struct buddy_free_block_node *)addr;

  free_block->node.item.item_void_ptr = &allocator->free_block_lists[order];
  free_block->order = order;

  list_insert_node_at_end(&allocator->free_block_lists[order], true, &free_block->node);
  free_block->checksum = buddy_calc_checksum(free_block);
  bitmap_set(&allocator->block_bitmap,
             buddy_get_block_number(allocator, free_block));
}

NON_NULL_PARAM(1) static int buddy_remove_free_block(struct buddy_allocator *allocator,
    struct buddy_free_block_node *free_block) {
  if(!buddy_is_checksum_valid(free_block)) {
    RET_MSG(E_FAIL, "Block checksum is not valid.");
  }

  list_unlink_node(&allocator->free_block_lists[free_block->order], &free_block->node);

  // Invalidate checksum of the removed block node

  free_block->checksum = buddy_calc_checksum(free_block) ^ ~0;

  // Set the block as invalid

  bitmap_clear(&allocator->block_bitmap,
               buddy_get_block_number(allocator, free_block));

  return E_OK;
}

NON_NULL_PARAMS static struct buddy_free_block_node *buddy_get_free_block(struct buddy_allocator *allocator,
    size_t order) {
  list_node_t *node;

  if(IS_ERROR(list_remove_node_from_end(&allocator->free_block_lists[order], true, &node)))
    return NULL;
  else
    return (struct buddy_free_block_node *)node;
}

int buddy_init(struct buddy_allocator *allocator, size_t mem_size,
               size_t min_order_size, void *mem_region, void *start_address) {
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
  allocator->lock = 0;

  for(size_t i = 0; i < orders; i++) {
    list_init(&allocator->free_block_lists[i]);
  }

  if(IS_ERROR(bitmap_init_at(&allocator->block_bitmap, mem_size / min_order_size,
                             mem_region, false))) {
    RET_MSG(E_FAIL, "Unable to initialize block bitmap.");
  }

  allocator->start_address = start_address;

  buddy_add_free_block(allocator, start_address, orders-1);

  return E_OK;
}

size_t buddy_block_order(const struct buddy_allocator *allocator,
                         size_t mem_size) {
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
                      struct memory_block *block) {
  size_t block_order = buddy_block_order(allocator, size);

  lock_acquire(&allocator->lock);

  /* Look for the smallest free block that is at least as large as the requested
   * allocation size. If found, remove it from the free array and return it. Otherwise,
   * repeatedly split blocks in half until a block of the desired size is obtained.
   */

  for(size_t order = block_order; order < allocator->orders; order++) {
    struct buddy_free_block_node *free_block = buddy_get_free_block(allocator, order);

    if(free_block != NULL) {
      if(IS_ERROR(buddy_remove_free_block(allocator, free_block))) {
        lock_release(&allocator->lock);
        RET_MSG(E_FAIL, "Unable to remove free block.");
      }

      block->size = MIN_ORDER_SIZE(allocator) << block_order;
      block->ptr = free_block;

      /* Split a large block into two halves, then split a half into two quarters,
       * split a quarter into two eights, and so on until a block of the desired
       * order remains.
       */

      for(; order > block_order; order--)
        buddy_add_free_block(allocator,
                             (void *)((uintptr_t)block->ptr ^ (1ul << (order - 1 + allocator->log2_min_order_size))),
                             order - 1);

      lock_release(&allocator->lock);

      return E_OK;
    }
  }

  /* A block of sufficient size couldn't be found. Determine
   * whether there's still usable memory left or if the system has
   * completely exhausted allocatable memory.
   */

  for(size_t i = 0; i < block_order; i++) {
    if(!is_list_empty(&allocator->free_block_lists[i])) {
      lock_release(&allocator->lock);
      RET_MSG(E_SIZE, "Requested block is too large.");
    }
  }

  lock_release(&allocator->lock);
  RET_MSG(E_OOM, "Out of memory.");
}

int buddy_reserve_block(struct buddy_allocator *allocator, const void *addr, size_t size,
                        struct memory_block *mem_block) {
  size_t block_order = buddy_block_order(allocator, size);
  unsigned long int block_num = (unsigned long int)((uintptr_t)addr >> (allocator->log2_min_order_size));

  /* Look to see if the block has been marked as free. If so, then
     simply remove it from the free block array. Otherwise, find the
     the smallest superblock that it belongs to and then continually split
     it until we get the reserved block. Finally, remove the reserved block.
  */

  lock_acquire(&allocator->lock);

  for(size_t order = block_order; order < allocator->orders; order++) {
    void *block_addr = (void *)((uintptr_t)addr & ~((1ul << (order + allocator->log2_min_order_size))-1));

    if(bitmap_is_set(&allocator->block_bitmap, buddy_get_block_number(allocator, block_addr))) {
      if(IS_ERROR(buddy_remove_free_block(allocator, (struct buddy_free_block_node *)block_addr))) {
        lock_release(&allocator->lock);
        RET_MSG(E_FAIL, "Unable to remove free block.");
      }

      /* Split a large block into two halves, then split a half into two quarters,
       * split a quarter into two eights, and so on until a block of the desired
       * order remains.
       */

      for(; order > block_order; order--)
        buddy_add_free_block(allocator,
                             (void *)((uintptr_t)block_addr ^ (1ul << (order - 1 + allocator->log2_min_order_size))),
                             order - 1);

      lock_release(&allocator->lock);

      return E_OK;
    }
  }

  lock_release(&allocator->lock);

  // The reserved block couldn't be found (because it was already allocated or reserved)

  RET_MSG(E_FAIL, "Unable to reserve block.");
}

int buddy_free_block(struct buddy_allocator *allocator,
                     const struct memory_block *block) {
  if(block->size
      > MIN_ORDER_SIZE(allocator) << (allocator->orders - 1)
      || block->size < MIN_ORDER_SIZE(allocator)) {
    RET_MSG(E_INVALID_ARG, "Invalid block size.");
  }

  if((uintptr_t)block->ptr < (uintptr_t)allocator->start_address
      || ((uintptr_t)block->ptr >= (uintptr_t)allocator->start_address
          + (1ul << (allocator->log2_min_order_size + allocator->orders))
          && ((uintptr_t)allocator->start_address
              + (1ul << (allocator->log2_min_order_size + allocator->orders)) != 0))) {
    RET_MSG(E_INVALID_ARG, "Invalid block address.");
  }

  size_t block_order = buddy_block_order(allocator, block->size);
  uintptr_t block_offset = (uintptr_t)block->ptr - (uintptr_t)allocator->start_address;

  lock_acquire(&allocator->lock);

  if(bitmap_is_set(&allocator->block_bitmap, buddy_get_block_number(allocator, (void *)block_offset))) {
    lock_release(&allocator->lock);
    RET_MSG(E_INVALID_ARG, "Attempted to free an already free block");
  }

  size_t order=block_order;

  for(; order < allocator->orders; order++) {
    uintptr_t buddy_offset = block_offset ^ (1ul << (order + allocator->log2_min_order_size));

    if(bitmap_is_set(&allocator->block_bitmap, buddy_get_block_number(allocator, (void *)buddy_offset))) {
      struct buddy_free_block_node *block_node = (struct buddy_free_block_node *)
      ((uintptr_t)allocator->start_address + buddy_offset);

      if(IS_ERROR(buddy_remove_free_block(allocator, block_node))) {
        lock_release(&allocator->lock);
        RET_MSG(E_FAIL, "Unable to remove free block.");
      }
    } else
      break;
  }

  buddy_add_free_block(allocator, block->ptr, order);

  lock_release(&allocator->lock);

  return E_OK;
}

size_t buddy_free_bytes(struct buddy_allocator *allocator) {
  size_t bytes = 0;

  lock_acquire(&allocator->lock);

  for(size_t i = 0; i < allocator->orders; i++)
    bytes += list_len(&allocator->free_block_lists[i]) << (i + allocator->log2_min_order_size);

  lock_release(&allocator->lock);

  return bytes;
}
