#ifndef LIBC_THREADS_H
#define LIBC_THREADS_H

#include <stdnoreturn.h>

typedef int(*thrd_start_t)(void*);

typedef struct {

} thrd_t;

int thrd_create(thrd_t *thr, thrd_start_t func, void *arg);
int thrd_equal(thrd_t lhs, thrd_t rhs);
thrd_t thrd_current(void);
int thrd_sleep(const struct timespec* duration,
                struct timespec* remaining);
void thrd_yield(void);
noreturn void thrd_exit(int res);
int thrd_detach(thrd_t thr);
int thrd_join(thrd_t thr, int *res);

enum {
    thrd_success = 0,
    thrd_nomem = -1,
    thrd_timedout = -2,
    thrd_busy = -3,
    thrd_error = -4
};

#endif /* LIBC_THREADS_H */
