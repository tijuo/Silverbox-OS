#include <time.h>
#include <os/variables.h>
#include <os/services.h>
#include <errno.h>

time_t time(time_t *timer)
{
  unsigned int t;

  if(!timer || getCurrentTime(&t) != 0)
  {
    errno = EFAULT;
    return (time_t)-1;
  }

  *timer = (time_t)t;

  return *timer;
}
