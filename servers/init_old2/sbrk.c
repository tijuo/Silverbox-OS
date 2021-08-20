#include <os/memory.h>
#include <os/syscalls.h>
#include "phys_mem.h"

#define MIN_INCREMENT	4*PAGE_SIZE
#define MFAIL                ((void*)-1)

void *heapStart=NULL;
void *heapEnd=NULL;
size_t heapSize=0;

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

    while(addr < (addr_t)heapEnd)
    {
      size_t count;
      pframe_t frames[128];

      for(count=0; count < 128 && addr + count * PAGE_SIZE < (addr_t)heapEnd; count++)
      {
        addr_t physPage = allocPhysPage();

        if(physPage == NULL)
        {
          while(count--)
            freePhysPage(frames[count]);

          return prevHeap;
        }

        frames[count] = (pframe_t)(physPage >> 12);
      }

      if(count)
      {
        int actuallyMapped = sys_map(NULL, (void *)addr, frames, count, PM_READ_WRITE | PM_ARRAY);

        if(count != (size_t)actuallyMapped)
        {
          int toUnmap = (actuallyMapped >= 0 ? count - actuallyMapped : count);

          for(int i=0; i < toUnmap; i++)
            freePhysPage(frames[count-i]);

          return prevHeap;
        }
      }

      addr += count * PAGE_SIZE;
    }
  }

  return prevHeap;
}
