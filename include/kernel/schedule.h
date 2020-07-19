#ifndef SCHEDULE_H
#define SCHEDULE_H

#include <kernel/thread.h>
#include <kernel/queue.h>

#define NUM_PRIORITIES		8u
#define NUM_RUN_QUEUES		NUM_PRIORITIES

#define HIGHEST_PRIORITY 	0u
#define NORMAL_PRIORITY 	4u
#define LOWEST_PRIORITY		(NUM_PRIORITIES - 1)

/** The number of run queues used for the scheduler. */

int attachPausedQueue( TCB *thread );
int detachPausedQueue( TCB *thread );
TCB *attachRunQueue( TCB *thread );
TCB *detachRunQueue( TCB *thread );

int setPriority( TCB *thread, unsigned int level );

HOT(TCB *schedule( TCB * ));
HOT(void timerInt( TCB * ));
HOT(dword *updateCurrentThread(TCB *tcb, ExecutionState state));
void idle(void);

extern struct Queue runQueues[NUM_RUN_QUEUES], timerQueue;

#endif /* SCHEDULE_H */
