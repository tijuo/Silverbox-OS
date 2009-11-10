#ifndef SCHEDULE_H
#define SCHEDULE_H

#include <kernel/thread.h>
#include <os/io.h>
#include <kernel/queue.h>

#define NUM_PRIORITIES		8

#define HIGHEST_PRIORITY 	0
#define NORMAL_PRIORITY 	4
#define LOWEST_PRIORITY		NUM_PRIORITIES - 1

/** The number of run queues used for the scheduler. */

int maxRunQueues;

int attachPausedQueue( TCB *thread );
int detachPausedQueue( TCB *thread );
int attachRunQueue( TCB *thread );
int detachRunQueue( TCB *thread );

HOT(TCB *schedule( volatile TCB *tcb ));
HOT(void timerInt(volatile TCB *_currThread));
void idle(void) __attribute__((noreturn));

#endif /* SCHEDULE_H */
