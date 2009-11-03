#ifndef PTHREAD_H
#define PTHREAD_H

typedef struct {
  struct RegisterState regs;
  int state, priority;
};

int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                   void * (*start_routine)(void *), void *arg);
vodi pthread_exit(void *status);
int pthread_join(pthread_t thread, void **status);
int pthread_once(pthread_once_t *once_init, void (*init_routine)(void));
int pthread_kill(pthread_t thread, int sig);
pthread_t pthread_self(void);
int pthread_equal(pthread_t thread1, pthread_t thread2);
void pthread_yield(void);
int pthread_detach(pthread_t thread);

/*
pthread_key_create
pthread_key_delete
pthread_getspecific
pthread_setspecific
pthread_cancel
pthread_cleanup_pop
pthread_cleanup_push
pthread_setcancelstate
pthread_getcancelstate
pthread_testcancel
pthread_getschedparam
pthread_setschedparam
pthread_sigmask
pthread_attr_init
pthread_attr_destroy
pthread_setdatachstate
pthread_attr_getdetachstate
pthread_attr_getstackaddr
pthread_attr_getstacksize
pthread_attr_setstackaddr
pthread_attr_setstacksize
pthread_attr_getschedparam
pthread_attr_setschedparam
pthread_attr_getschedpolicy
pthread_attr_setschedpolicy
pthread_attr_setinheritsched
pthread_attr_getinheritsched
pthread_attr_setscope
pthread_attr_getscope
pthread_mutex_init
pthread_mutex_destroy
pthread_mutex_lock
pthread_mutex_unlock
pthread_mutex_trylock
pthread_mutex_setprioceiling
pthread_mutex_getprioceiling
pthread_mutexattr_init
pthread_mutexattr_destroy
pthread_mutexattr_getpshared
pthread_mutexattr_setpshared
pthread_mutexattr_getprotocol
pthread_mutexattr_setprotocol
pthread_mutexattr_setprioceiling
pthread_mutexattr_getprioceiling
pthread_cond_init
pthread_cond_destroy
pthread_cond_signal
pthread_cond_broadcast
pthread_cond_wait
pthread_cond_timedwait
pthread_condattr_init
pthread_condattr_destroy
pthread_condattr_getpshared
pthread_condattr_setpshared
*/

#endif /* PTHREAD_H */
