#include <oslib.h>
#include <os/memory.h>
#include <os/services.h>
#include <errno.h>

size_t heapSize;
void *heapStart, *heapEnd, *mapEnd;

#define ZERO_DEV	0x00000001u
#define MIN_MAP_PAGES	16

//extern void mapVirt( void *virt, int pages );

extern void print(char *str);

/* XXX: This doesn't handle negative increments. */

void *sbrk( int increment )
{
  void *prevHeapEnd = heapEnd;
  size_t pages=0;

  errno = 0;

  if(!heapEnd)
  {
    size_t startAddr = (size_t)HEAP_START;

    heapEnd = (void *)((startAddr & (PAGE_SIZE-1)) == 0 ? startAddr : (startAddr & ~(PAGE_SIZE-1)) + PAGE_SIZE);
    heapStart = heapEnd;
    mapEnd = heapStart;
  }

  if( increment == 0 )
    return prevHeapEnd;

  heapEnd = (void *)((unsigned)heapEnd + increment);
  heapSize += increment;

  if(heapEnd > mapEnd)
  {
    pages = ((size_t)heapEnd / PAGE_SIZE) - ((size_t)mapEnd / PAGE_SIZE) + 1;
    pages = (pages < MIN_MAP_PAGES ? MIN_MAP_PAGES : pages);
  }

  if( pages > 0 )
  {
    if(increment > 0)
    {
      if( mapMem((addr_t)mapEnd, ZERO_DEV, PAGE_SIZE*pages, 0, 0) != 0 )
      {
        errno = -ENOMEM;
        return (void *)-1;
      }
      else
      {
        mapEnd = (void *)((size_t)mapEnd + pages * PAGE_SIZE);
      }
    }
    else if(increment < 0)
    {
      // XXX: Unmap pages
    }
  }

  return prevHeapEnd;
}
