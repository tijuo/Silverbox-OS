#include <time.h>
#include <os/variables.h>

time_t time(time_t *timer)
{
  time_t t = (time_t)((*(clktick_t *)CLOCK_TICKS) / TICKS_PER_SEC);

  if( timer )
    *timer = t;

  return t;
}
