#include <kernel/thread.h>
#include <kernel/debug.h>
#include <kernel/memory.h>
#include <kernel/schedule.h>
#include <kernel/paging.h>
#include <kernel/lowlevel.h>
#include <kernel/error.h>
#include <kernel/list.h>

list_t runQueues[NUM_PRIORITIES];

// Assumes processor id is valid

RETURNS_NON_NULL
tcb_t* schedule(proc_id_t processorId)
{
  tcb_t *currentThread = processors[processorId].runningThread;
  int minPriority = currentThread ? currentThread->priority : MIN_PRIORITY;

  for(int priority = MAX_PRIORITY; minPriority; priority--) {
    if(!isListEmpty(&runQueues[priority])) {
      tcb_t *newThread = listDequeue(&runQueues[priority]);

      // If the currently running thread has been preempted, then
      // simply place it back onto its run queue

      if(currentThread) {
        currentThread->threadState = READY;
        listEnqueue(&runQueues[currentThread->priority], currentThread);
      }

      newThread->threadState = RUNNING;
      processors[processorId].runningThread = newThread;

      return newThread;
    }
  }

  if(currentThread)
    return currentThread;
  else
    // todo: Have a minimum priority kernel idle thread that does nothing
    panic("No more threads to run.");
}

/**
 Switch to a new stack (and thus perform a context switch to a new thread),
 if necessary.

 @param The saved execution state of the processor.
 */

NON_NULL_PARAMS void switchStacks(ExecutionState *state) {
  tcb_t *oldTcb = getCurrentThread();

  if(!oldTcb || oldTcb->threadState != RUNNING) {
    tcb_t *newTcb = schedule(getCurrentProcessor());

    if(newTcb != oldTcb && newTcb) // if switching to a new thread
    {
//      kprintf("switchStacks: %u -> %u\n", getTid(oldTcb), getTid(newTcb));

      // Switch to the new address space

      if((getCR3() & CR3_BASE_MASK) != (newTcb->rootPageMap & CR3_BASE_MASK))
        setCR3(newTcb->rootPageMap);

      if(oldTcb) {
        oldTcb->userExecState = *state;
        __asm__("fxsave %0" :: "m"(oldTcb->xsaveState));
      }

      asm volatile("fxrstor %0\n" :: "m"(newTcb->xsaveState));

      *state = newTcb->userExecState;
    }
    else if(!newTcb)
      panic("No more threads to schedule.");
  }
}
