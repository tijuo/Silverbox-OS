#ifndef QUEUE_H
#define QUEUE_H

#include <types.h>
#include <kernel/thread.h>

TCB *timerDetach( TCB *thread );
TCB *timerEnqueue( TCB *thread, unsigned int time );
TCB *timerPop( void );
TCB *enqueue( struct Queue *queue, TCB *thread );
TCB *detachQueue( struct Queue *queue, TCB *thread );
TCB *popQueue( struct Queue *queue );
int isInQueue( const struct Queue *queue, const TCB *thread );
int isInTimerQueue( const TCB *thread );

#endif /* QUEUE_H */
