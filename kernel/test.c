#include <kernel/debug.h>
#include <kernel/memory.h>

void testMemset(void);

void testMemset(void)
{
  int diff1, diff2;
  static char area[8192];

  /* Does memset set all bytes properly? */

  startTimeStamp();
  for(int i=0; i < 100; i++)
    memset(area, 0x00, 8192);
  stopTimeStamp();

  diff1 = getTimeDifference();

  startTimeStamp();
  for(int j=0; j < 100; j++)
    _memset(area, 0xFF, 8192);
  stopTimeStamp();

  diff2 = getTimeDifference();

  kprintf("memcpy() - %d ticks. %d ticks avg\n", diff1, diff1 / 100);
  kprintf("_memcpy() - %d ticks. %d ticks avg\n", diff2, diff2 / 100);
}
