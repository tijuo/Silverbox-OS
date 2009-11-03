#ifndef SEMAPHORE_H
#define SEMAPHORE_H

struct Semaphore
{
  void *resource;
  int  value;
};

#define INIT_SEMAPHORE(x, addr, val)    static struct Semaphore x ## Lock = { addr, val }

#define SEM_AQUIRE(x, num)
#define SEM_RELEASE(x, num)
#endif
