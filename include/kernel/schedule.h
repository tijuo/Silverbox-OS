#ifndef KERNEL_SCHEDULE_H
#define KERNEL_SCHEDULE_H

#include <kernel/lowlevel.h>
#include <kernel/thread.h>
#include <kernel/types/list.h>

#define NUM_PRIORITIES          5
#define MIN_PRIORITY            0
#define MAX_PRIORITY            4
#define NORMAL_PRIORITY         2

tcb_t* schedule(proc_id_t processor_id);
HOT void switch_stacks(exec_state_t *state);

extern list_t run_queues[NUM_PRIORITIES];

#endif /* KERNEL_SCHEDULE_H */
