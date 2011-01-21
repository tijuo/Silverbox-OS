#ifndef QUEUE_H
#define QUEUE_H

#include <types.h>
#include <kernel/thread.h>

tid_t detachQueue( struct Queue *queue, tid_t thread );
int enqueue( struct Queue *queue, tid_t thread );
tid_t popQueue( struct Queue *queue );
bool isInQueue( struct Queue *queue, tid_t tid );
bool isInTimerQueue( tid_t tid );
int timerDetach( tid_t tid );
int timerEnqueue( tid_t tid, unsigned short time );
tid_t timerPop( void );

#endif /* QUEUE_H */
