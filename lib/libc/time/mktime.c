#include <time.h>

time_t mktime(struct tm *timestruct)
{
  time_t t = 0;

  if( timestruct == NULL )
    return 0;

  t += timestruct->tm_sec;
  t += timestruct->tm_min * 60;
  t += timestruct->tm_hour * 60 * 60;
  t += timestruct->tm_yday * 60 * 60 * 24;

  for(int i=1970; i < timestruct->tm_year; i++)
    t += (_isleap(i) ? 366 : 365) * 60 * 60 * 24;

  return t;
}
