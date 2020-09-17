#ifndef KERNEL_SCHEDULE_H
#define KERNEL_SCHEDULE_H

#include <kernel/thread.h>
#include <kernel/list.h>

/** The number of run queues used for the scheduler. */

#define NUM_PRIORITIES		7
#define NUM_RUN_QUEUES		NUM_PRIORITIES

#define HIGHEST_PRIORITY 	(NUM_PRIORITIES - 1)
#define NORMAL_PRIORITY 	(NUM_PRIORITIES / 2)
#define LOWEST_PRIORITY		0

#define MSECS_PER_QUANTUM   1000

int attachPausedQueue( tcb_t *thread );
int detachPausedQueue( tcb_t *thread );
tcb_t *attachRunQueue( tcb_t *thread );
tcb_t *detachRunQueue( tcb_t *thread );

int setPriority( tcb_t *thread, unsigned int level );

HOT(tcb_t *schedule(void));
HOT(void timerInt(UNUSED_PARAM ExecutionState *state));
HOT(void switchStacks(ExecutionState *state));

extern list_t runQueues[NUM_RUN_QUEUES];
extern list_t timerQueue;

#endif /* KERNEL_SCHEDULE_H */
