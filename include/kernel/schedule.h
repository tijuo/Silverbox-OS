#ifndef KERNEL_SCHEDULE_H
#define KERNEL_SCHEDULE_H

#include <kernel/lowlevel.h>
#include <kernel/thread.h>

#define NUM_PRIORITIES          5
#define MIN_PRIORITY            0
#define MAX_PRIORITY            4
#define NORMAL_PRIORITY         2

tcb_t* schedule(proc_id_t processorId);
HOT void switchStacks(ExecutionState *state);

extern list_t runQueues[NUM_PRIORITIES];

#endif /* KERNEL_SCHEDULE_H */
