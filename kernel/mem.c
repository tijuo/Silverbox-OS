#include <kernel/mm.h>
#include <kernel/paging.h>
#include <kernel/error.h>
#include <kernel/debug.h>
#include <oslib.h>

addr_t heapEnd = NULL;

paddr_t *freePageStack=(paddr_t *)PAGE_STACK;
paddr_t *freePageStackTop;

bool tempMapped;

static void freeUnusedHeapPages(void);

/** Allocate an available physical page frame.
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

  return (freePageStackTop == freePageStack) ? NULL_PADDR : *--freePageStackTop;
}

/** Release a page frame.
    @param frame The physical address of the page frame to be released
**/

void freePageFrame(paddr_t frame)
{
  assert((frame & (PAGE_SIZE - 1)) == 0);

  if(freePageStackTop == NULL)
    *freePageStackTop++ = frame & ~(PAGE_SIZE - 1);
}

void freeUnusedHeapPages(void)
{
  addr_t addr=heapEnd;

  // Align address to the next page boundary, if unaligned

  if((addr & (PAGE_SIZE-1)) != 0)
    addr += PAGE_SIZE - (addr & (PAGE_SIZE - 1));

  // Unmap and free any present pages that are still mapped past the end of heap

  while(addr < KERNEL_HEAP_LIMIT)
  {
    pde_t *pde = ADDR_TO_PDE(addr);

    if(pde->present)
    {
      pte_t *pte = ADDR_TO_PTE(addr);

      if(pte->present)
      {
        freePageFrame((paddr_t)(pte->base << 12));
        pte->base = 0;
        pte->present = 0;
      }

      addr += PAGE_SIZE;
    }
    else // If an entire page table is marked as not-present, then assume that all PTEs are invalid (and thus have no allocated pages). Skip searching through it
      addr += PAGE_TABLE_SIZE - (addr & (PAGE_TABLE_SIZE - 1));
  }

  /*
  // Do not unmap and free any empty page tables. They'll still be
  // mapped in other address spaces.

  addr = heapEnd;

  if((addr & (PAGE_TABLE_SIZE-1)) != 0)
    addr += PAGE_TABLE_SIZE - (addr & (PAGE_TABLE_SIZE - 1));

  for(; addr < KERNEL_HEAP_LIMIT; addr += PAGE_TABLE_SIZE)
  {
    pde_t *pde = ADDR_TO_PDE(addr);

    if(pde->present)
    {
      freePageFrame((paddr_t)(pde->base << 12));
      pde->base = 0;
      pde->present = 0;
    }
  }
  */
}

/** Extend/shrink the kernel heap by a set amount.

    @param amt The amount by which to extend the heap (if positive) or shrink the heap (if negative).
    @return The end of the kernel heap.
**/

void *morecore(int amt)
{
  if(amt == 0)
    return (!heapEnd ? MFAIL : (void *)heapEnd);

  if(!heapEnd)
    heapEnd = KERNEL_HEAP_START;

  addr_t prevHeapEnd = heapEnd;

  if(amt > 0)
  {
/*
    if(amt < MIN_AMT)
      amt = MIN_AMT;
*/
    // Ensure that we're mapping in multiples of pages

    if((amt & (PAGE_SIZE - 1)) != 0)
      amt += PAGE_SIZE - (amt & (PAGE_SIZE - 1));

    if(heapEnd + amt > KERNEL_HEAP_LIMIT || heapEnd + amt < heapEnd) // overflow becomes underflow
      heapEnd = KERNEL_HEAP_LIMIT;
    else
      heapEnd += amt;
  }
  else if(amt < 0) // Shrink the kernel heap.
  {
    if(heapEnd + amt < KERNEL_HEAP_START
       || heapEnd + amt > heapEnd) // underflow becomes overflow
      heapEnd = KERNEL_HEAP_START;
    else
      heapEnd += amt;
  }

  return (void *)prevHeapEnd;
}
