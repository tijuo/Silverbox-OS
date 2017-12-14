#include <kernel/debug.h>
#include <kernel/memory.h>

void testMemset(void);

extern void *_memset(void *, int, size_t);

void testMemset(void)
{
  unsigned int diff1, diff2;
  static char area[8192];
  int i;

  /* Does memset set all bytes properly? */

  startTimeStamp();
  for(i=0; i < 100; i++)
    memset(area, 0x00, 8192);
  stopTimeStamp();

  diff1 = getTimeDifference();

  startTimeStamp();
  for(i=0; i < 100; i++)
    _memset(area, 0xFF, 8192);
  stopTimeStamp();

  diff2 = getTimeDifference();

  kprintf("memcpy() - %d ticks. %d ticks avg\n", diff1, diff1 / 100);
  kprintf("_memcpy() - %d ticks. %d ticks avg\n", diff2, diff2 / 100);
}
