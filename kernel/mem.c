#include <kernel/mm.h>
#include <kernel/paging.h>
#include <kernel/error.h>
#include <kernel/debug.h>
#include <oslib.h>

paddr_t *freePageStack=(paddr_t *)PAGE_STACK;
paddr_t *freePageStackTop=NULL;
bool tempMapped=false;
size_t pageTableSize = PAGE_TABLE_SIZE;

static void freeUnusedHeapPages(void);

addr_t heapEnd = NULL;

void *memset(void *ptr, int value, size_t num)
{
  char *p = (char *)ptr;

  while(num)
  {
    *(p++) = value;
    num--;
  }

  return ptr;
}

// XXX: Assumes that the memory regions don't overlap

void *memcpy(void *dest, const void *src, size_t num)
{
  char *d = (char *)dest;
  const char *s = (const char *)src;

  while(num)
  {
    *(d++) = *(s++);
    num --;
  }

  return dest;
}

/** Allocate an available 4 KB physical page frame.

    @return The physical address of a newly allocated page frame. NULL_PADDR, on failure.
 **/

paddr_t allocPageFrame(void)
{
#if DEBUG
  if(!freePageStackTop)
    RET_MSG(NULL_PADDR, "allocPageFrame(): Free page stack hasn't been initialized yet.");
#endif /* DEBUG */

  // Attempt to reclaim unused heap pages

  if(freePageStackTop == freePageStack)
  {
    PRINT_DEBUG("allocPageFrame(): Free page stack is empty!\n");
    freeUnusedHeapPages();
  }

#if DEBUG
  else
    assert(*(freePageStackTop-1) == ALIGN_DOWN(*(freePageStackTop-1), PAGE_SIZE))
#endif /* DEBUG */

    if(freePageStackTop == freePageStack)
      RET_MSG(NULL_PADDR, "Kernel has no more available physical page frames.");
    else
      return *--freePageStackTop;
}

/** Release a 4 KB page frame.

    @param frame The physical address of the page frame to be released
 **/

void freePageFrame(paddr_t frame)
{
  assert(frame == ALIGN_DOWN(frame, PAGE_SIZE));
  assert(freePageStackTop);

#if DEBUG
  if(freePageStackTop)
#endif /* DEBUG */
    *freePageStackTop++ = ALIGN_DOWN(frame, PAGE_SIZE);

#if DEBUG
  else
    PRINT_DEBUG("Free page stack hasn't been initialized yet\n");
#endif /* DEBUG */
}

/**
  Attempt to release any physical page frames that have been allocated for heap use that
  are not being used.
 */

void freeUnusedHeapPages(void)
{
  addr_t addr=heapEnd;
  pde_t pde;
  pte_t pte;
  int firstPass = 1;

  // Align address to the next page boundary, if unaligned

  addr = ALIGN_UP(addr, PAGE_SIZE);

  // Unmap and free any present pages that are still mapped past the end of heap

  while(addr < KERNEL_HEAP_LIMIT)
  {
    if((IS_ALIGNED(addr, pageTableSize) || firstPass)
        && IS_ERROR(readPmapEntry(NULL_PADDR, addr, &pde)))
    {
      addr = ALIGN_UP(addr, pageTableSize);
      continue;
    }

    firstPass = 0;

    if(pde.present && !IS_ERROR(readPmapEntry((paddr_t)PFRAME_TO_ADDR(pde.base), addr, &pte)))
    {
      if(pte.present)
      {
        freePageFrame((paddr_t)PFRAME_TO_ADDR(pte.base));
        pte.base = 0;
        pte.present = 0;

        invalidatePage(addr);
      }

      addr += PAGE_SIZE;
    }
    else // If an entire page table is marked as not-present, then assume that all PTEs are invalid (and thus have no allocated pages). Skip searching through it
      addr = ALIGN_UP(addr, pageTableSize);
  }
}
