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

int attachPausedQueue( tcb_t *thread );
int detachPausedQueue( tcb_t *thread );
tcb_t *attachRunQueue( tcb_t *thread );
tcb_t *detachRunQueue( tcb_t *thread );

int setPriority( tcb_t *thread, unsigned int level );

HOT(tcb_t *schedule( tcb_t * ));
HOT(void timerInt( tcb_t * ));
HOT(dword *updateCurrentThread(tcb_t *tcb, ExecutionState state));

extern struct Queue runQueues[NUM_RUN_QUEUES], timerQueue;

#endif /* SCHEDULE_H */
