#include <kernel/thread.h>
#include <kernel/debug.h>
#include <kernel/memory.h>
#include <kernel/schedule.h>
#include <kernel/pic.h>
#include <kernel/paging.h>
#include <kernel/lowlevel.h>

extern void init2( void );
extern int numBootMods;
void systemThread( void );

struct Queue runQueues[NUM_RUN_QUEUES], timerQueue;

int setPriority( TCB *thread, unsigned int level );
TCB *attachRunQueue( TCB *thread );
TCB *detachRunQueue( TCB *thread );

void idle(void)
{
  addr_t addr;

  disableInt();
  kprintf("\n0x%x bytes of discardable code.", (addr_t)EXT_PTR(kdData) - (addr_t)EXT_PTR(kdCode));
  kprintf(" 0x%x bytes of discardable data.\n", (addr_t)EXT_PTR(kBss) - (addr_t)EXT_PTR(kdData));
  kprintf("Discarding %d bytes in total\n", (addr_t)EXT_PTR(kBss) - (addr_t)EXT_PTR(kdCode));

  /* Get rid of the code and data that will never be used again. */

  for( addr=(addr_t)EXT_PTR(kdCode); addr < (addr_t)EXT_PTR(kBss); addr += PAGE_SIZE )
    kUnmapPage( (addr_t)addr, NULL );

  kUnmapPage( (addr_t)EXT_PTR(ioPermBitmap), NULL );
  kUnmapPage( (addr_t)EXT_PTR(ioPermBitmap) + PAGE_SIZE, NULL );

  enableInt();

  while(1)
    __asm__("hlt\n");
}

/* TODO: There's probably a more efficient way of scheduling threads */

/**
  Picks the best thread to run next. Picks the idle thread, if none are available.

  This scheduler picks a thread according to priority. Higher priority threads
  will always run before lower priority threads (given that they are ready-to-run).
  Higher priority threads are given shorter time slices than lower priority threads.
  This assumes that high priority threads are IO-bound threads and lower priority
  threads are CPU-bound. If a thread uses up too much of its time slice, then its
  priority level is decreased. If there are no threads to run, then run the idle
  thread.

  @todo Add a way to increase the priority level of threads if it uses very little of
        its time slice.

  @param thread The TCB of the current thread.
  @return A pointer to the TCB of the newly scheduled thread on success. NULL on failure.
*/

TCB *schedule( TCB *thread )
{
  unsigned int priority, priorityLimit=LOWEST_PRIORITY;
  TCB *newThread = NULL;

  assert( thread != NULL );
  assert( GET_TID(thread) != NULL_TID );

  if( thread == NULL )
    return NULL;

  /* Threads with lower priority numbers *MUST* execute before threads
     with higher priority numbers. Warning: This may cause starvation
     if incorrectly set. */

  assert( thread == currentThread );

  if( thread->threadState == RUNNING )
    thread->threadState = READY;

  if( thread->threadState == READY )
    priorityLimit = thread->priority;

  for( priority=HIGHEST_PRIORITY; priority <= priorityLimit; priority++ )
  {
    if( (newThread=popQueue(&runQueues[priority])) != NULL )
      break;
  }

  if( !newThread ) // If no other threads are found (the previously running thread is the only one in its queue that's ready)
  {
    if( thread->threadState == READY ) // and the current thread is ready to run, then run it
    {
      currentThread = newThread = thread;
    }
    else
    {
      kprintf("Unable to schedule any threads! (including the idle thread)\n");
      assert(false);
    }
  }
  else                      // If a thread was found, then run it(assumes that the thread is ready since it's in the ready queue)
  {
    assert( newThread->threadState == READY );

    currentThread = newThread;

    if( thread->threadState == READY )
    {
      #ifdef DEBUG
        assert( attachRunQueue( thread ) == thread );
      #else
        attachRunQueue( thread );
      #endif
    }
  }

  incSchedCount();

  newThread->threadState = RUNNING;
  newThread->quantaLeft = newThread->priority + 1;

  return newThread;
}

dword *updateCurrentThread(TCB *tcb, ExecutionState state)
{
  if(tcb != NULL && tcb->threadState != RUNNING)
  {
    TCB *newTcb = schedule(tcb);

    if(newTcb == tcb)
      return (dword *)&state;
    else
    {
      if((tcb->cr3 & 0xFFFFF000u) != (newTcb->cr3 & 0xFFFFF000u))
        __asm__ __volatile__("mov %0, %%cr3" :: "r"(newTcb->cr3));

      tcb->kernel = (state.user.cs == KCODE);

      if(state.user.cs == KCODE)
      {
        dword *stackTop = ((dword *)&tcb) - 1;
        *stackTop = tcb->execState.user.edi;
        tcb->execState.kernel.stackTop = (dword)stackTop;
      }

      *(dword *)EXT_PTR(tssEsp0) = (dword)(((ExecutionState *)&newTcb->execState) + 1);

      if(newTcb->kernel)
      {
        dword *stackTop = (dword *)newTcb->execState.kernel.stackTop;
        newTcb->execState.user.edi = *stackTop;
        newTcb->kernel = ((ExecutionState *)stackTop)->user.cs == KCODE;
        return stackTop+1;
      }
      else
        return (dword *)&newTcb->execState;
    }
  }
  else
    return (dword *)&state;
}

/**
    Adjusts the priority level of a thread.

    If the thread is already on a run queue, then it will
    be detached from its current run queue and attached to the
    run queue associated with the new priority level.

    @param thread The thread of a TCB.
    @param level The new priority level.
    @return 0 on success. -1 on failure.
*/

int setPriority( TCB *thread, unsigned int level )
{
  assert( thread != NULL );
  assert( GET_TID(thread) != NULL_TID );
  assert( level < NUM_PRIORITIES );
  assert( thread != idleThread || level == LOWEST_PRIORITY );

  if( thread == NULL )
    return -1;

  if( level >= NUM_PRIORITIES )
    return -1;

  if( thread == idleThread && level != LOWEST_PRIORITY )
    return -1;

  if( thread->threadState == READY )
    detachRunQueue( thread );

  thread->priority = level;

  if( thread->threadState == READY )
    attachRunQueue( thread );

  if( level < currentThread->priority && currentThread->threadState == RUNNING )
    currentThread->threadState = READY;

  return 0;
}

/**
    Adds a thread to the run queue.

    @param thread The TCB of the thread to attach.
    @return The TCB of the attached thread. NULL on failure.
*/


TCB *attachRunQueue( TCB *thread )
{
  assert( thread != NULL );
  assert( GET_TID(thread) != NULL_TID );
  assert( thread->priority < NUM_PRIORITIES && thread->priority >= 0 );
  assert( thread->threadState == READY );

  assert( thread != currentThread );

  if( thread->threadState != READY )
    return NULL;

  // If a higher priority thread than the currently running thread is added to the run queue,
  // then indicate this to the scheduler

  if( thread->priority < currentThread->priority )
  {
    kprintf("Preempting thread: %d with thread: %d\n", GET_TID(currentThread), GET_TID(thread));
  }

  return enqueue( &runQueues[thread->priority], thread );
}

/**
    Removes a thread from the run queue.

    @param thread The TCB of the thread to detach.
    @return The TCB of the detached thread. NULL on failure or if the thread isn't on a run queue
*/

TCB *detachRunQueue( TCB *thread )
{
  assert( thread != NULL );
  assert( thread->priority < NUM_PRIORITIES );

  if( thread == NULL )
    return NULL;

  if( thread->threadState == RUNNING )
    return NULL;

  assert( thread != NULL );
  assert( GET_TID(thread) != NULL_TID );

  return detachQueue( &runQueues[ thread->priority ], thread );
}

/**
  Handles a timer interrupt.

  @note This function should only be called by low-level IRQ handler routines.

  The timer interrupt handler does periodic things like updating kernel
  clocks/timers and decreasing a thread's quanta.

  @note Since this is an interrupt handler, there should not be too much
        code here and it should execute quickly.

  @param thread The TCB of the current thread.
*/

void timerInt( TCB *thread )
{
    TCB *wokenThread;

    incTimerCount();

    assert( thread != NULL );
    assert( GET_TID(thread) != NULL_TID );
    assert( thread->threadState == RUNNING );
    assert( thread->quantaLeft );

    thread->quantaLeft--;

    if( thread->quantaLeft )
    {
      thread->quantaLeft--;
    }

    assert( timerQueue.head == NULL || timerQueue.head->queue.delta );

    if( timerQueue.head && timerQueue.head->queue.delta )
      timerQueue.head->queue.delta--;

    while( timerQueue.head && timerQueue.head->queue.delta == 0 )
    {
      wokenThread = timerPop();

      assert( wokenThread != NULL );
      assert( wokenThread->threadState != READY && wokenThread->threadState != RUNNING );

      assert( !isInTimerQueue( wokenThread ) );

      if( wokenThread->threadState == WAIT_FOR_SEND || wokenThread->threadState == WAIT_FOR_RECV )
      {
        kprintf("SIGTMOUT to %d\n", GET_TID(wokenThread));
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

    sendEOI();
}
