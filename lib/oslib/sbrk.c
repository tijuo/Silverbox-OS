#include <oslib.h>
#include <os/memory.h>
#include <os/services.h>
#include <errno.h>

//extern void mapVirt( void *virt, int pages );

extern void print(char *str);

/* XXX: This doesn't handle negative increments. */

void *sbrk( int increment )
{
  void *prevHeap = heapEnd;
  int pages;

  errno = 0;

  if( heapStart == NULL )
  {
    heapStart = heapEnd = prevHeap = (void *)HEAP_START;

    if( allocatePages((void *)heapStart, 1 ) < 0 )
    {
      errno = -ENOMEM;
      return (void *)-1;
    }
  }

  if( increment == 0 )
    return prevHeap;

  heapEnd = (void *)((unsigned)heapEnd + increment);
  heapSize += increment;

  pages = ((unsigned)heapEnd / PAGE_SIZE) - ((unsigned)prevHeap / PAGE_SIZE);

  if( pages > 0 )
  {
    if( allocatePages( (void *)(((unsigned)prevHeap & ~(PAGE_SIZE - 1)) + PAGE_SIZE), pages ) < 0 )
    {
      errno = -ENOMEM;
      return (void *)-1;
    }
  }
  else if( pages < 0 )
  {
    // XXX: unmap pages?
  }

  return prevHeap;
}
