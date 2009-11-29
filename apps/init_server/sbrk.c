#include <oslib.h>
#include <os/memory.h>
#include <os/services.h>

extern void mapMemRange( void *virt, int pages );

/* This doesn't handle negative increments. */

void *sbrk( int increment )
{
  void *prevHeap = heapEnd;
  int pages;

  if( heapStart == NULL )
  {
    heapStart = heapEnd = prevHeap = (void *)HEAP_START;
    mapMemRange((void *)heapStart, 1);
  }

  if( increment == 0 )
    return prevHeap;

  heapEnd += increment;
  heapSize += increment;

  pages = ((unsigned)heapEnd / PAGE_SIZE) - ((unsigned)prevHeap / PAGE_SIZE);

  if( pages > 0 )
     mapMemRange((void *)(((unsigned)prevHeap & ~(PAGE_SIZE - 1)) + PAGE_SIZE), pages);

  return prevHeap;
}
