#ifndef QUEUE_H
#define QUEUE_H

#include <types.h>
#include <kernel/thread.h>

int sleepEnqueue( struct Queue *queue, tid_t tid, unsigned short time );
tid_t sleepDequeue( struct Queue *queue );
int enqueue( struct Queue *queue, tid_t thread );
tid_t detachQueue( struct Queue *queue, tid_t thread );
tid_t dequeue( struct Queue *queue );
#endif /* QUEUE_H */