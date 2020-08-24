#include <kernel/thread.h>
#include <kernel/debug.h>
#include <kernel/memory.h>
#include <kernel/schedule.h>
#include <kernel/pic.h>
#include <kernel/paging.h>
#include <kernel/lowlevel.h>
#include <kernel/error.h>

queue_t runQueues[NUM_RUN_QUEUES], timerQueue;

int setPriority( tcb_t *thread, unsigned int level );
tcb_t *attachRunQueue( tcb_t *thread );
tcb_t *detachRunQueue( tcb_t *thread );

/* TODO: There's probably a more efficient way of scheduling threads */

/**
  Picks the best thread to run next.

  This scheduler picks a thread according to priority. Higher priority threads
  will always run before lower priority threads (given that they are ready-to-run).
  Higher priority threads are given shorter time slices than lower priority threads.
  This assumes that high priority threads are IO-bound threads and lower priority
  threads are CPU-bound. If a thread uses up too much of its time slice, then its
  priority level is decreased.

  @return A pointer to the TCB of the newly scheduled thread on success. NULL on failure.
*/

tcb_t *schedule(void)
{
  int priority;
  tcb_t *newThread = NULL, *oldThread = currentThread;

  /* Threads with higher priority *MUST* execute before threads
     with lower priority. Warning: This may cause starvation
     if incorrectly set. */

  if(oldThread)
  {
    if( oldThread->threadState == RUNNING )
      oldThread->threadState = READY;
  }

  for( priority=HIGHEST_PRIORITY; priority >= LOWEST_PRIORITY; priority-- )
  {
    if( !isQueueEmpty(&runQueues[priority])
        && !IS_ERROR(queueDequeue(&runQueues[priority], (void **)&newThread)) )
    {
      assert(newThread != NULL);
      break;
    }
  }

  if(!newThread && !oldThread)
  {
    kprintf("Unable to schedule any threads!\n");
    return NULL;
  }
  else if(newThread) // If a thread was found, then run it(assumes that the thread is ready since it's in the ready queue)
  {
    assert( newThread->threadState == READY );

    currentThread = newThread;

    if( oldThread && oldThread->threadState == READY )
    {
      #ifdef DEBUG
        assert( attachRunQueue( oldThread ) == oldThread );
      #else
        attachRunQueue( oldThread );
      #endif
    }
  }
  else
  {
    assert(oldThread != NULL);
    newThread = oldThread;
  }

  newThread->threadState = RUNNING;
  newThread->quantaLeft = newThread->priority + 1;

  incSchedCount();

  return newThread;
}

/**
  Switch to a new stack (and thus perform a context switch to a new thread),
  if necessary.

  @param The saved execution state of the processor.
*/

void switchStacks(ExecutionState *state)
{
  assert(state != NULL);
  // No need to switch stacks if going back into kernel code

  if(state->cs == KCODE)
    return;

  tcb_t *oldTcb = currentThread;

  if((oldTcb && (oldTcb->threadState != RUNNING || !oldTcb->quantaLeft)))
  {
    tcb_t *newTcb = schedule();

    if(newTcb && newTcb != oldTcb)
    {
      if((oldTcb->rootPageMap & 0xFFFFF000u) != (newTcb->rootPageMap & 0xFFFFF000u))
        __asm__ __volatile__("mov %0, %%cr3" :: "r"(newTcb->rootPageMap));

      oldTcb->execState = *state;
      *state = newTcb->execState;
    }
  }
}

/**
    Adjust the priority level of a thread.

    If the thread is already on a run queue, then it will
    be detached from its current run queue and attached to the
    run queue associated with the new priority level.

    @param thread The thread of a TCB.
    @param level The new priority level.
    @return E_OK on success. E_FAIL on failure. E_DONE if priority would remain unchanged.
*/

int setPriority( tcb_t *thread, unsigned int level )
{
  assert( thread != NULL );
  assert( level < NUM_PRIORITIES );

  if(thread->priority == level)
    return E_DONE;

  if(thread->threadState == READY)
    detachRunQueue( thread );

  thread->priority = level;

  if( thread->threadState == READY && !attachRunQueue(thread))
    return E_FAIL;

  if( level < currentThread->priority && currentThread->threadState == RUNNING )
    currentThread->threadState = READY;

  return E_OK;
}

/**
    Adds a thread to the run queue.

    @param thread The TCB of the thread to attach.
    @return The TCB of the attached thread. NULL on failure.
*/


tcb_t *attachRunQueue( tcb_t *thread )
{
  assert( thread != NULL );
  assert( thread->priority <= HIGHEST_PRIORITY && thread->priority >= LOWEST_PRIORITY );
  assert( thread->threadState == READY );

  if( thread->threadState != READY )
    return NULL;

  return (IS_ERROR(queueEnqueue(&runQueues[thread->priority], getTid(thread),
              thread)) ? NULL : thread);
}

/**
    Removes a thread from the run queue.

    @param thread The TCB of the thread to detach.
    @return The TCB of the detached thread. NULL on failure or if the thread isn't on a run queue
*/

tcb_t *detachRunQueue( tcb_t *thread )
{
  assert( thread != NULL );
  assert( thread->priority < NUM_PRIORITIES );

  return (IS_ERROR(queueRemoveFirst(&runQueues[ thread->priority ],
                   getTid(thread), NULL)) ? NULL : thread);
}

/**
  Handles a timer interrupt.

  @note This function should only be called by low-level IRQ handler routines.

  The timer interrupt handler does periodic things like updating kernel
  clocks/timers and decreasing a thread's quanta.

  @note Since this is an interrupt handler, there should not be too much
        code here and it should execute quickly.

  @param state The saved execution state of the processor.
*/

void timerInt(UNUSED_PARAM ExecutionState *state)
{
  tcb_t *wokenThread;
  timer_delta_t *timerDelta;

  if( currentThread && currentThread->quantaLeft )
    currentThread->quantaLeft--;

  incTimerCount();

  if(!isQueueEmpty(&timerQueue))
  {
    timerDelta = (timer_delta_t *)queueGetHead(&timerQueue);

    if(timerDelta->delta)
      timerDelta->delta--;

    for(; !isQueueEmpty(&timerQueue) && timerDelta->delta == 0; )
    {
      queuePop(&timerQueue, (void **)&timerDelta);

      wokenThread = timerDelta->thread;

      if(!isQueueEmpty(&timerQueue))
        timerDelta = (timer_delta_t *)queueGetHead(&timerQueue);

      assert( wokenThread != NULL );
      assert( wokenThread->threadState != READY && wokenThread->threadState != RUNNING );

      assert( queueFindFirst(&timerQueue, getTid(wokenThread), NULL) == E_FAIL );

      if( wokenThread->threadState == WAIT_FOR_SEND || wokenThread->threadState == WAIT_FOR_RECV )
      {
        kprintf("SIGTMOUT to %d\n", getTid(wokenThread));
        //sysRaise(wokenThread, SIGTMOUT, 0);
      }
      else
      {
        wokenThread->threadState = READY;

        #ifndef DEBUG
          attachRunQueue( wokenThread );
        #else
          assert( attachRunQueue( wokenThread ) != NULL );
        #endif
      }

      assert( wokenThread != currentThread );
    }
  }
  sendEOI();
}
