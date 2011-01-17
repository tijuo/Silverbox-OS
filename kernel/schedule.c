#include <kernel/thread.h>
#include <oslib.h>
#include <kernel/debug.h>
#include <kernel/memory.h>
#include <kernel/schedule.h>
#include <kernel/signal.h>

extern void init2( void );
extern int numBootMods;
void systemThread( void );
TCB *idleThread;

int sysYield( TCB *thread );
int setPriority( TCB *thread, int level );
int attachRunQueue( TCB *thread );
int detachRunQueue( TCB *thread );
/*
int attachPausedQueue( TCB *thread );
int detachPausedQueue( TCB *thread );
*/

/** There is a specific reason why this isn't a regular C function. It has to do
    with the way the compiler compiles C functions and stack memory. The idle thread
    code has to be specifically like this. */

__asm__(".globl idle\n" \
        "idle: \n" \
        "cli\n" \
        "call init2\n" \
        "sti\n" \
        ".halt: hlt\n" \
        "jmp .halt\n");
/*
void idle( void )
{
  disableInt();

  / * Do the second part of initialization here * /

  init2();

  enableInt();
  while ( true )
    asm("hlt\n");
}
*/

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

TCB *schedule( volatile TCB *thread )
{
  int level;
  tid_t newTID;
  TCB *newThread;

  assert( thread != NULL );

  if( thread == NULL )
    return NULL;

  /* Higher priorities *MUST* execute before lower priorities. */
  /* Warning: This may cause starvation if incorrectly set. */

  /* Why would the previously running thread's state ever be READY? */

  assert( thread == currentThread );
  assert( thread->state != READY );

  if( thread->state == RUNNING || thread->state == READY )
  {
     if( thread != idleThread )
     {
        // Set the priority
        if( thread->quantaLeft == 0 && thread->priority < NUM_PRIORITIES - 1 )
        {
          if( thread->state == READY )
            detachRunQueue( (TCB *)thread );

          thread->priority++;

          if( thread->state == READY )
            attachRunQueue( (TCB *)thread );
        }
     }

    thread->state = READY;

    if( thread == idleThread )
      thread->quantaLeft = 1;
    else
    {
      thread->quantaLeft = thread->priority + 1;
    //  attachRunQueue( thread );
    }
  }

//  if( thread != idleThread && thread->state != READY ) /* Why ? */
//    assert(false);

  for( level=HIGHEST_PRIORITY; level < NUM_PRIORITIES; level++ ) // Cycle through the queues to find a queue with ready threads
  {
    assert( (runQueues[level].head == NULL_TID && runQueues[level].tail == NULL_TID)
          || (runQueues[level].head != NULL_TID && runQueues[level].tail != NULL_TID) );

    if( runQueues[ level ].tail != NULL_TID || runQueues[ level ].head != NULL_TID ) // The || is used instead of && to do the assert checking
    {
      assert( !isInQueue( &runQueues[level], GET_TID(thread) ) ); /* The previously running thread should NOT be in the run(ready-to-run) queue */
      break;
    }
  }

  if( level >= NUM_PRIORITIES ) // If no other threads are found(the previously running thread is the only one that is ready)
  {
    if( thread->state == READY && thread != idleThread ) // and the current thread is ready to run, then run it
    {
      currentThread = thread;
      newThread = (TCB *)currentThread;
      assert( thread != idleThread );
    }
    else
    {
      currentThread = idleThread;
      newThread = (TCB *)currentThread;        // otherwise run the idle thread
    }
  }
  else                      // If a thread was found, then run it(assumes that the thread is ready since it's in the ready queue)
  {
    newTID = dequeue( &runQueues[ level ] );

    assert( newTID != NULL_TID );

    currentThread = &tcbTable[newTID];
    newThread = (TCB *)currentThread;

    assert( newThread->state == READY );

    if( thread->state == READY && thread != idleThread )
    {
      #ifdef DEBUG
        assert( attachRunQueue( (TCB *)thread ) == 0 );
      #else
        attachRunQueue( (TCB *)thread );
      #endif
    }
  }

  assert( newThread != NULL );

  currentThread->state = newThread->state = RUNNING;

  incSchedCount();
  return newThread;
}

/// Voluntarily gives up control of the current thread

int sysYield( TCB *thread )
{
  assert( thread != NULL );

  if( thread == NULL )
    return -1;

  assert(thread == currentThread);

//  schedule( thread );
  thread->quantaLeft = 0; // Not sure if this actually works
  return 0;
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
int setPriority( TCB *thread, int level )
{
  assert( thread != NULL );

  if( thread == NULL || thread == idleThread )
    return -1;

  if( level < 0 || level >= NUM_PRIORITIES )
    return -1;

  assert(GET_TID(thread) != NULL_TID );

  if( thread->state == READY )
    detachRunQueue( thread );

    //  if( thread != currentThread )
    thread->priority = level;

  if( thread->state == READY )
    attachRunQueue( thread );

  return 0;
}

/** 
    Adds a thread to the run queue.

    @param thread The TCB of the thread to attach.
    @return 0 on success. -1 on success. 1 if the thread was running (and shouldn't be on a run queue)
*/
int attachRunQueue( TCB *thread )
{
  assert( thread != NULL );
  assert( thread->priority < NUM_PRIORITIES );
  assert( thread->state == READY );

  if( thread == NULL )
    return -1;

  assert( thread != currentThread ); /* If the current thread is attached to its run queue, the scheduler will break! */

  if( thread->state == RUNNING )
    return 1;

  #if DEBUG
  int ret;

  ret = enqueue( &runQueues[ thread->priority ], GET_TID(thread) );

  return ret;
  #else
    return enqueue( &runQueues[thread->priority ], GET_TID(thread) );
  #endif
}

/**
    Removes a thread from the run queue.

    @param thread The TCB of the thread to detach.
    @return 0 on success. -1 on failure. 1 if the thread is running (and shouldn't be on a run queue).
*/

int detachRunQueue( TCB *thread )
{
  assert( thread != NULL );
  assert( thread->priority < NUM_PRIORITIES );

  if( thread == NULL )
    return -1;

  if( thread->state == RUNNING )
    return 1;

  assert( GET_TID(thread) != NULL_TID );

  detachQueue( &runQueues[ thread->priority ], GET_TID(thread) );

  return 0;
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

void timerInt( volatile TCB *thread )
{
    TCB *wokenThread;/*, *thread = (TCB *)(*tssEsp0 - sizeof(Registers) - sizeof(dword));*/
    tid_t tid;

    incTimerCount();

    assert( thread != NULL );

    thread->quantaLeft--;

    if( timerQueue.head != NULL_TID )
      timerNodes[timerQueue.head].delta--;

    while( timerQueue.head != NULL_TID && timerNodes[timerQueue.head].delta == 0 )
    {
      tid = timerPop();

      assert( tid != NULL_TID );
      wokenThread = &tcbTable[tid];
      assert( wokenThread->state != READY && wokenThread->state != RUNNING );

      assert( !isInTimerQueue( tid ) );

      if( wokenThread->state == WAIT_FOR_SEND || wokenThread->state == WAIT_FOR_RECV )
      {
        kprintf("SIGTMOUT to %d\n", GET_TID(wokenThread));
        sysRaise(wokenThread, SIGTMOUT, 0);
      }
      else
      {
        wokenThread->state = READY;

        #ifndef DEBUG
          attachRunQueue( wokenThread );
        #else
          assert( attachRunQueue( wokenThread ) == 0 );
        #endif
      }

      assert( wokenThread != currentThread );

      wokenThread->quantaLeft = wokenThread->priority + 1; // This is important. Scheduler breaks without this for some reason?
    }

    sendEOI();
}
