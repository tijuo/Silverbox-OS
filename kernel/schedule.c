#include <kernel/debug.h>
#include <kernel/error.h>
#include <kernel/list.h>
#include <kernel/lowlevel.h>
#include <kernel/memory.h>
#include <kernel/paging.h>
#include <kernel/schedule.h>
#include <kernel/thread.h>
#include <x86intrin.h>

list_t run_queues[NUM_PRIORITIES];

// Assumes processor id is valid

RETURNS_NON_NULL
tcb_t* schedule(proc_id_t processor_id)
{
    tcb_t* current_thread = processors[processor_id].running_thread;
    int min_priority = current_thread ? current_thread->priority : MIN_PRIORITY;

    for(int priority = MAX_PRIORITY; priority >= min_priority; priority--) {
        if(!LIST_IS_EMPTY(&run_queues[priority])) {
            tcb_t* new_thread = list_dequeue(&run_queues[priority]);

            // If the currently running thread has been preempted, then
            // simply place it back onto its run queue

            if(current_thread) {
                current_thread->thread_state = READY;
                list_enqueue(&run_queues[current_thread->priority], current_thread);
            }

            new_thread->thread_state = RUNNING;
            processors[processor_id].running_thread = new_thread;

            return new_thread;
        }
    }

    if(current_thread) {
        return current_thread;
    }
    else {
        // todo: Have a minimum priority kernel idle thread that does nothing
        PANIC("No more threads to run.");
        return NULL;
    }
}

/**
 Switch to a new stack (and thus perform a context switch to a new thread),
 if necessary.

 @param The saved execution state of the processor.
 */
NON_NULL_PARAMS void switch_stacks(ExecutionState* state)
{
    tcb_t* old_tcb = thread_get_current();

    if(!old_tcb || old_tcb->thread_state != RUNNING) {
        tcb_t* new_tcb = schedule(processor_get_current());

        if(new_tcb != old_tcb && new_tcb) // if switching to a new thread
        {
            //      kprintf("switchStacks: %u -> %u\n", getTid(oldTcb), getTid(newTcb));

            // Switch to the new address space

            if((get_cr3() & CR3_BASE_MASK) != (new_tcb->root_pmap & CR3_BASE_MASK)) {
                set_cr3(new_tcb->root_pmap);
            }

            if(old_tcb) {
                old_tcb->user_exec_state = *state;

                if(old_tcb->fxsave_state) {
                    _fxsave(old_tcb->fxsave_state);
                }
            }

            if(new_tcb->fxsave_state) {
                _fxrstor(old_tcb->fxsave_state);
            }

            *state = new_tcb->user_exec_state;
        } else if(!new_tcb) {
            PANIC("No more threads to schedule.");
        }
    }
}
