#include <threads.h>
#include <time.h>
#include <os/syscalls.h>

int thrd_create(thrd_t *thr, thrd_start_t func, void *arg);

int thrd_equal(thrd_t lhs, thrd_t rhs);

thrd_t thrd_current(void);

int thrd_sleep(const struct timespec *duration, struct timespec *remaining) {
  if(!duration)
    return -1;

  sys_sleep(duration.tv_sec * 1000);

  if(remaining) {
    remaining->tv_sec = 0;
    remaining->tv_nsec = 0;
  }

  return 0;
}

void thrd_yield(void) {
  sys_sleep(0);
}

_Noreturn void thrd_exit(int res) {
  sys_exit(res);
}

int thrd_detach(thrd_t thr);
int thrd_join(thrd_t thr, int *res);

