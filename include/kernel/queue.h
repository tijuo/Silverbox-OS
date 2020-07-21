#ifndef QUEUE_H
#define QUEUE_H

#include <types.h>
#include <kernel/thread.h>

tcb_t *timerDetach( tcb_t *thread );
tcb_t *timerEnqueue( tcb_t *thread, unsigned int time );
tcb_t *timerPop( void );
tcb_t *enqueue( struct Queue *queue, tcb_t *thread );
tcb_t *detachQueue( struct Queue *queue, tcb_t *thread );
tcb_t *popQueue( struct Queue *queue );
int isInQueue( const struct Queue *queue, const tcb_t *thread );
int isInTimerQueue( const tcb_t *thread );

#endif /* QUEUE_H */
