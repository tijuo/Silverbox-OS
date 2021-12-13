#include <x86intrin.h>
#include "buddy.h"

#include <stdio.h>
#include <string.h>
#include <kernel/pae.h>

int buddies_init(struct buddies *restrict buddies, size_t mem_size,
				 void *restrict mem_region)
{
  if(mem_size < MIN_ORDER_SIZE)
	RET_MSG(E_FAIL, "Memory size must be at least 4096 bytes.");

  unsigned int orders = (unsigned int)__bsrq(mem_size);

  if(orders != (unsigned int)__bsfq(mem_size))
	orders++;

  orders -= MIN_ORDER_BITS - 1;

  if(orders >= 34)
	RET_MSG(E_FAIL,
			"Number of block orders must be greater than 0 and less than 34");

  buddies->orders = orders;
  buddies->free_counts = (uint32_t*)mem_region;
  buddies->free_blocks = (uint32_t**)&buddies->free_counts[orders];

  uint32_t *blocks = (uint32_t*)&buddies->free_blocks[orders];

  for(size_t i = 0, offset = 0; i < orders; i++, offset += 1 << i)
	buddies->free_blocks[orders - i - 1] = (uint32_t*)blocks + offset;

  memset(buddies->free_counts, 0, orders * sizeof(uint32_t));

  buddies->free_blocks[orders - 1][0] = 0;
  buddies->free_counts[orders - 1] = 1;

  return E_OK;
}

NON_NULL_PARAMS unsigned int buddies_block_order(struct buddies *buddies,
										   size_t mem_size)
{
  if(mem_size <= MIN_ORDER_SIZE)
	return 0;
  else if(mem_size >= (MIN_ORDER_SIZE << (buddies->orders - 1)))
	return buddies->orders - 1;
  else {
	unsigned int msb_index = (unsigned int)__bsrq(mem_size);

	if(msb_index != (unsigned int)__bsfq(mem_size))
	  msb_index++;

	return msb_index - MIN_ORDER_BITS;
  }
}

/// Allocate a new block of `size` bytes. Returns a tuple consisting of the address
/// of the newly allocated block and actual size of the block on success. Returns
/// `AllocError` on failure.

int buddies_alloc_block(struct buddies *buddies, size_t size,
						struct memory_block *block)
{
  unsigned int block_order = buddies_block_order(buddies, size);

  // Look for the smallest free block that is at least as large as the requested
  // allocation size. If found, remove it from the free array and return it. Otherwise,
  // repeatedly split blocks in half until a block of the desired size is obtained.

  for(unsigned int order = block_order; order < buddies->orders; order++) {
	if(buddies->free_counts[order] > 0) {
	  uint32_t free_block =
		  buddies->free_blocks[order][buddies->free_counts[order] - 1];

	  // Split a large block into two halves, then split a half into two quarters,
	  // split a quarter into two eights, and so on until a block of the desired
	  // order remains.

	  while(order > block_order) {
		buddies->free_counts[order] -= 1;

		uint32_t next_size = buddies->free_counts[order - 1];
		uint32_t *next_array = buddies->free_blocks[order - 1];

		next_array[next_size] = 2 * free_block + 1;
		next_array[next_size + 1] = 2 * free_block;

		buddies->free_counts[order - 1] += 2;
		free_block *= 2;
		order -= 1;
	  }

	  buddies->free_counts[order] -= 1;

	  block->size = MIN_ORDER_SIZE << block_order;
	  block->ptr =
		  (void*)(buddies->free_blocks[order][buddies->free_counts[order]]
			  * block->size);

	  return E_OK;
	}
  }

  // If no free blocks were found, then coalesce smaller blocks into larger ones
  // and try to allocate memory again

  buddies_coalesce_blocks(buddies);

  if(buddies->free_counts[block_order] != 0)
	return buddies_alloc_block(buddies, size, block);
  else {
	for(size_t i = 0; i < buddies->orders; i++) {
	  if(buddies->free_counts[i] > 0)
		RET_MSG(E_SIZE, "Requested block is too large.");
	}
  }

  RET_MSG(E_OOM, "Out of memory.");
}

int buddies_free_block(struct buddies *buddies, struct memory_block *block) {
  if(block->size
	  > MIN_ORDER_SIZE << (buddies->orders - 1)|| block->size < MIN_ORDER_SIZE)
  {
	RET_MSG(E_INVALID_ARG, "Invalid block size.");
  }

  unsigned int block_order = buddies_block_order(buddies, block->size);
  uint32_t block_number = (uint32_t)((uintptr_t)block->ptr
	  / (uintptr_t)(1 << block_order)*MIN_ORDER_SIZE);

  if(block_number >= (1 << (buddies->orders - 1 - block_order)))
	RET_MSG(E_INVALID_ARG, "Invalid block.");

  buddies->free_blocks[block_order][buddies->free_counts[block_order]++] =
	  block_number;

  buddies_coalesce_blocks(buddies);

  return E_OK;
}

int buddies_block_cmp(const void *b1, const void *b2) {
  uint32_t *bl1 = (uint32_t*)b1;
  uint32_t *bl2 = (uint32_t*)b2;

  return *bl1 < *bl2 ? -1 : !!(*bl1 - *bl2);
}

bool buddies_coalesce_blocks(struct buddies *buddies) {
  bool did_coalesce = false;

  for(unsigned int order = 0; order < buddies->orders; order++) {
	uint32_t shift = 0;
	uint32_t free_blocks = buddies->free_counts[order];

	qsort(buddies->free_blocks[order], (size_t)free_blocks, sizeof(uint32_t),
			   buddies_block_cmp);

	for(uint32_t i = 0; i < free_blocks;) {
	  uint32_t coalesced_blk;
	  uint32_t *array = buddies->free_blocks[order];

	  if(i + 1 < free_blocks && array[i] + 1 == array[i + 1]
		  && array[i] % 2 == 0) {
		did_coalesce = true;
		coalesced_blk = array[i] / 2;
	  }
	  else {
		array[i - shift] = array[i];
		i += 1;
		continue;
	  }

	  uint32_t larger_count = buddies->free_counts[order + 1];

	  buddies->free_blocks[order + 1][larger_count] = coalesced_blk;
	  buddies->free_counts[order + 1] += 1;
	  shift += 2;
	  i += 2;
	}

	buddies->free_counts[order] -= shift;
  }

  return did_coalesce;
}

size_t buddies_free_bytes(struct buddies *buddies) {
  size_t bytes = 0;

  for(size_t i=0; i < buddies->orders; i++) {
	bytes += buddies->free_counts[i] * (1 << i)*MIN_ORDER_SIZE;
  }

  return bytes;
}
