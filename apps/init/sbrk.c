#include <os/memory.h>
#include <os/syscalls.h>
#include "phys_mem.h"

#define MIN_INCREMENT	4*PAGE_SIZE
#define MFAIL                ((void*)-1)

void *heapStart, *heapEnd;
size_t heapSize;

/* XXX: This doesn't unmap memory due to negative increments. */

void *sbrk( int increment )
{
  void *prevHeap = heapEnd;

  if( increment == 0 )
    return heapEnd ? prevHeap : MFAIL;
  else if(increment > 0)
  {
    if(!heapEnd)
      heapStart = heapEnd = prevHeap = (void *)HEAP_START;

    if(increment < MIN_INCREMENT)
      increment = MIN_INCREMENT;
    else
    {
      if(increment & (PAGE_SIZE-1))
        increment += PAGE_SIZE - (increment & (PAGE_SIZE - 1)); // make the increment in multiples of the page size
    }
  }

  if( increment < 0 && (unsigned)-increment > heapSize )
  {
    heapEnd = (void *)HEAP_START;
    heapSize = 0;
  }
  else if(increment > 0
          && ((addr_t)heapEnd + increment > HEAP_LIMIT
              || (addr_t)heapEnd + increment < (addr_t)HEAP_START))
  {
    heapEnd = (void *)HEAP_LIMIT;
    heapSize = (size_t)HEAP_LIMIT - (size_t)HEAP_START;
  }
  else
  {
    heapEnd = (void *)((unsigned)heapEnd + increment);
    heapSize += increment;
  }

  if(increment > 0)
  {
    addr_t addr = (addr_t)prevHeap;

    if((addr_t)prevHeap & (PAGE_SIZE-1))
      prevHeap = (void *)((addr_t)prevHeap + PAGE_SIZE - ((addr_t)prevHeap & (PAGE_SIZE-1)));

    for(; addr < (addr_t)heapEnd; addr += PAGE_SIZE)
      sys_map(NULL, addr, (pframe_t)(allocPhysPage() >> 12), 1/*((addr_t)heapEnd-addr) / PAGE_SIZE*/, PM_READ_WRITE);
  }

  return prevHeap;
}
