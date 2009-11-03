#ifndef OS_MUTEX_H
#define OS_MUTEX_H

typedef volatile char mutex_t;

extern int mutex_lock( mutex_t * );
extern int mutex_unlock( mutex_t * );

#endif /* OS_MUTEX_H */
