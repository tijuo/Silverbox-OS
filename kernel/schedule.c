#include <kernel/thread.h>
#include <oslib.h>
#include <kernel/debug.h>
#include <kernel/memory.h>
#include <kernel/schedule.h>

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
    if( runQueues[ level ].tail != NULL_TID || runQueues[ level ].head != NULL_TID ) // The || is used instead of && to do the assert checking
    {
      assert( runQueues[ level ].head != GET_TID(thread) ); /* The previously running thread should NOT be in the run(ready-to-run) queue */
      assert( runQueues[ level ].tail != GET_TID(thread) );
      assert( runQueues[ level ].head != NULL_TID );
      assert( runQueues[ level ].tail != NULL_TID );
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

/* Ok */

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

/* Ok */
int attachRunQueue( TCB *thread )
{
  assert( thread != NULL );
  assert( thread->priority < NUM_PRIORITIES );

  if( thread == NULL )
    return -1;

  assert( thread != currentThread ); /* If the current thread is attached to it's run queue, the scheduler will break! */

  if( thread->state == RUNNING )
    return 1;

  #if DEBUG
  int ret;

  ret = enqueue( &runQueues[ thread->priority ], GET_TID(thread) );
  assert( runQueues[ thread->priority ].head != NULL_TID );
  assert( runQueues[ thread->priority ].tail != NULL_TID );
  return ret;
  #else
    return enqueue( &runQueues[thread->priority ], GET_TID(thread) );
  #endif
}

/* Ok */
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

/* Ok */

void timerInt( volatile TCB *thread )
{
    TCB *wokenThread;/*, *thread = (TCB *)(*tssEsp0 - sizeof(Registers) - sizeof(dword));*/
    tid_t tid;

    incTimerCount();

    assert( thread != NULL );

    thread->quantaLeft--;

    /* TODO: Put a sleepDequeue() that dequeues a thread only if the head has 0 delta.
       It will also return the TID. Use the TID number to wake it up. Repeat until
       there is no head or the head's delta > 0. */

    if( sleepQueue.head != NULL_TID )
      tcbNodes[sleepQueue.head].delta--;

    while( sleepQueue.head != NULL_TID && tcbNodes[sleepQueue.head].delta == 0 )
    {
      tid = sleepDequeue( &sleepQueue );

      assert( tid != NULL_TID );
      wokenThread = &tcbTable[tid];
      assert( wokenThread->state == SLEEPING );

      #ifndef DEBUG
        attachRunQueue( wokenThread );
      #else
        assert( attachRunQueue( wokenThread ) == 0 );
      #endif
      assert( wokenThread != currentThread );
      wokenThread->state = READY;
      wokenThread->quantaLeft = wokenThread->priority + 1; // This is important. Scheduler breaks without this for some reason?
    }

    sendEOI();
}
