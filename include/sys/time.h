#ifndef SYS_TIME_H
#define SYS_TIME_H

struct timeval
{
  long tv_sec;          /* seconds */
  long tv_usec;         /* microseconds */
};

struct itimerval
{
  struct timeval it_interval;
  struct timeval it_value;
};

struct timezone {
    int tz_minuteswest;     /* minutes west of Greenwich */
    int tz_dsttime;         /* type of DST correction */
};

int gettimeofday(struct timeval *restrict tv,
                struct timezone *restrict tz);
int settimeofday(const struct timeval *tv,
                const struct timezone *tz);

#endif /* SYS_TIME_H */
