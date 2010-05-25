#ifndef SYS_TIME_H
#define SYS_TIME_H

struct timeval
{
  long tv_sec;
  long tv_usec;
};

struct itimerval
{
  struct timeval it_interval;
  struct timeval it_value;
};

#endif /* SYS_TIME_H */
