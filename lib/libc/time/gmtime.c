#include <time.h>
#include <errno.h>

extern struct tm __timestruct;

static const int month_days[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

struct tm *gmtime(const time_t *timer)
{
  time_t t;

  if( timer == NULL )
  {
    errno = EFAULT;
    return NULL;
  }

  t = *timer;

  __timestruct.tm_isdst = 0;
  __timestruct.tm_year = 70;

  __timestruct.tm_sec = (int)(t % 60);
  t /= 60;
  __timestruct.tm_min = (int)(t % 60);
  t /= 60;
  __timestruct.tm_hour = (int)(t % 24);
  t /= 24;

  __timestruct.tm_mon = __timestruct.tm_yday = 0;
  __timestruct.tm_wday = 4;

  while( t >= (_isleap(1900 + __timestruct.tm_year) ? 366 : 365) )
  {
    t -= (_isleap(1900 + __timestruct.tm_year) ? 366 : 365);
    __timestruct.tm_wday = (__timestruct.tm_wday + (_isleap(1900 + __timestruct.tm_year) ? 366 : 365)) % 7;
    __timestruct.tm_year++;
  }

  __timestruct.tm_yday = (int)t;

  for(int i=0; i < 12; i++,__timestruct.tm_mon++)
  {
    if( t < (unsigned)(month_days[i] + (i == 1 && _isleap(1900 + __timestruct.tm_year) ? 1 : 0)) )
    {
      __timestruct.tm_wday = (__timestruct.tm_wday + (int)t) % 7;
      __timestruct.tm_mday = 1 + (int)t;
      break;
    }
    else
    {
      __timestruct.tm_wday = (__timestruct.tm_wday + (i == 1 && _isleap(1900 + __timestruct.tm_year) ? 1 : 0)) % 7;
      __timestruct.tm_mday = 1 + (i == 1 && _isleap(1900 + __timestruct.tm_year) ? 1 : 0);
      t -=  (month_days[i] + (i == 1 && _isleap(1900 + __timestruct.tm_year) ? 1 : 0));
    }
  }

  return &__timestruct;
}
