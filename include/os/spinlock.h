#ifndef OS_SPINLOCK_H
#define OS_SPINLOCK_H

#include <os/mutex.h>
#include <os/syscalls.h>

#define SPINLOCK_INIT(name)     SPINLOCK_NAME(name) = 0

#define SPINLOCK_NAME(name)     name ## _spinlock

#define LOCKED_RES(type, name)  \
  mutex_t SPINLOCK_NAME(name) __attribute__((aligned (sizeof(int)))); \
  type name

#define SPINLOCK_WAIT(name)                 \
    while(!mutex_lock(&SPINLOCK_NAME(name)))    \
      sys_sleep(0);

#define SPINLOCK_RELEASE(name)          \
    while(mutex_unlock(&SPINLOCK_NAME(name)));

#endif /* OS_SPINLOCK_H */
