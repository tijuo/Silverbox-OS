#include <types.h>
#include <kernel/kmalloc.h>
#include <kernel/mm.h>
#include <kernel/debug.h>

#define BLK_LEN         16

addr_t heapEnd = NULL;

struct Block
{
  struct Block *nextFree;
  char data[BLK_LEN];
};

typedef struct Block block_t;

block_t *freeBlockHead;

void *kmalloc(size_t size)
{
  block_t *block;
  int hasFreeBlock = (freeBlockHead != NULL);

  if(size < 16)
    size = 16;

  assert(size == 16)

  // XXX: This allocator doesn't currently handle requests for more than 16 bytes

  if(size > 16)
    return NULL;

  if(hasFreeBlock)
    block = freeBlockHead;
  else
  {
    if(!heapEnd)
      heapEnd = KERNEL_HEAP_START;

    block = (block_t *)heapEnd;

    addr_t newEnd = heapEnd + sizeof(block_t);

    if(newEnd > KERNEL_HEAP_LIMIT || newEnd <= heapEnd)
      return NULL;
    else
      heapEnd = newEnd;
  }

  if(hasFreeBlock)
    freeBlockHead = block->nextFree;

  block->nextFree = NULL;

  return &block->data;
}

void kfree(void *p, UNUSED_PARAM size_t size)
{
  block_t *block = (block_t *)((addr_t)p - sizeof(block_t *));

  block->nextFree = freeBlockHead;
  freeBlockHead = block;
}

