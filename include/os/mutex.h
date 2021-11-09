#ifndef OS_MUTEX_H
#define OS_MUTEX_H

typedef volatile int mutex_t;

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

extern int mutex_lock( mutex_t * );
extern int mutex_is_locked( mutex_t * );
extern int mutex_unlock( mutex_t * );

#ifdef __cplusplus
};
#endif /* __cplusplus */
#endif /* OS_MUTEX_H */
