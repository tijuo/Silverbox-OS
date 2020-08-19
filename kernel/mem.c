#include <kernel/mm.h>
#include <kernel/paging.h>
#include <kernel/error.h>
#include <kernel/debug.h>
#include <oslib.h>

paddr_t *freePageStack=(paddr_t *)PAGE_STACK;
paddr_t *freePageStackTop;

bool tempMapped;

static void freeUnusedHeapPages(void);

/** Allocate an available 4 KB physical page frame.

    @return The physical address of a newly allocated page frame. NULL_PADDR, on failure.
**/

paddr_t allocPageFrame(void)
{
  if(freePageStackTop == NULL)
  {
    kprintf("allocPageFrame(): Free page stack hasn't been initialized yet!\n");
    return NULL_PADDR;
  }

  // Attempt to reclaim unused heap pages

  if(freePageStackTop == freePageStack)
    freeUnusedHeapPages();

  if(freePageStackTop == freePageStack)
  {
    kprintf("allocPageFrame(): Free page stack is empty!\n");
  }

#if DEBUG
  if(freePageStackTop != freePageStack)
    assert(((*(freePageStackTop-1)) & (PAGE_SIZE-1)) == 0);
#endif /* DEBUG */

  return (freePageStackTop == freePageStack) ? NULL_PADDR : *--freePageStackTop;
}

/** Release a 4 KB page frame.

    @param frame The physical address of the page frame to be released
**/

void freePageFrame(paddr_t frame)
{
  assert((frame & (PAGE_SIZE - 1)) == 0);

  if(freePageStackTop == NULL)
    *freePageStackTop++ = frame & ~(PAGE_SIZE - 1);
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

  // Align address to the next page boundary, if unaligned

  if((addr & (PAGE_SIZE-1)) != 0)
    addr += PAGE_SIZE - (addr & (PAGE_SIZE - 1));

  // Unmap and free any present pages that are still mapped past the end of heap

  for(int first=1; addr < KERNEL_HEAP_LIMIT; )
  {
    if((((addr & (PAGE_TABLE_SIZE-1)) == 0) || first) && IS_ERROR(readPmapEntry(NULL_PADDR, addr, &pde)))
    {
      addr = (addr & ~(PAGE_TABLE_SIZE-1)) + PAGE_TABLE_SIZE;
      continue;
    }

    first = 0;

    if(pde.present && !IS_ERROR(readPmapEntry((paddr_t)(pde.base << 12), addr, &pte)))
    {
      if(pte.present)
      {
        freePageFrame((paddr_t)(pte.base << 12));
        pte.base = 0;
        pte.present = 0;

        invalidatePage(addr);
      }

      addr += PAGE_SIZE;
    }
    else // If an entire page table is marked as not-present, then assume that all PTEs are invalid (and thus have no allocated pages). Skip searching through it
      addr += PAGE_TABLE_SIZE - (addr & (PAGE_TABLE_SIZE - 1));
  }
}
