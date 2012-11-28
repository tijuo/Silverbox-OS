#ifndef TIME_H
#define TIME_H

#include <types.h>
#include <stddef.h>
#include <os/variables.h>

#define CLOCKS_PER_SEC		TICKS_PER_SEC

#define _isleap(year)	((((year) % 4 == 0) && ((year) % 100 != 0)) || (year) % 400 == 0)

typedef u64 time_t;
typedef u64 clock_t;

struct tm
{
  int tm_sec; 	// 0-61
  int tm_min; 	// 0-59
  int tm_hour;	// 0-23
  int tm_mday;	// 1-31
  int tm_mon;   // 0-11 (0 = Jan)
  int tm_year;  // years since 1900
  int tm_wday;  // 0-6 (0 = sunday)
  int tm_yday;  // 0-365
  int tm_isdst;
};

clock_t clock(void);
time_t time(time_t *);
double difftime(time_t,time_t);
time_t mktime(struct tm *);

char *asctime(const struct tm *);
char *ctime(const time_t *);
struct tm *gmtime(const time_t *);
struct tm *localtime(const time_t *);
size_t strftime(char *, size_t, const char *, const struct tm *);

#endif /* TIME_H */
