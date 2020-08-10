#ifndef SCHEDULE_H
#define SCHEDULE_H

#include <kernel/thread.h>
//#include <kernel/queue.h>

#define NUM_PRIORITIES		7
#define NUM_RUN_QUEUES		NUM_PRIORITIES

#define HIGHEST_PRIORITY 	(NUM_PRIORITIES - 1)
#define NORMAL_PRIORITY 	3
#define LOWEST_PRIORITY		0

/** The number of run queues used for the scheduler. */

int attachPausedQueue( tcb_t *thread );
int detachPausedQueue( tcb_t *thread );
tcb_t *attachRunQueue( tcb_t *thread );
tcb_t *detachRunQueue( tcb_t *thread );

int setPriority( tcb_t *thread, unsigned int level );

HOT(tcb_t *schedule(void));
HOT(void timerInt(UNUSED_PARAM ExecutionState *state));
HOT(void switchStacks(ExecutionState *state));

extern queue_t runQueues[NUM_RUN_QUEUES], timerQueue;

#endif /* SCHEDULE_H */
