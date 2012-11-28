#include <kernel/debug.h>
#include <string.h>
#include <kernel/thread.h>
#include <kernel/schedule.h>
#include <kernel/mm.h>
#include <kernel/pit.h>

#include <oslib.h>

//extern void saveAndSwitchContext( TCB *, TCB * );

/** Starts a non-running thread

    The thread is started by placing it on a run queue.

    @param thread The TCB of the thread to be started.
    @return 0 on success. -1 on failure. 1 if the thread doesn't need to be started.
*/
int startThread( TCB *thread )
{
  #if DEBUG
  if( isInTimerQueue(thread) )
    kprintf("Thread is in timer queue!\n");
  #endif /* DEBUG */

  assert( !isInTimerQueue(thread) );

  if( thread->threadState == PAUSED ) /* A paused queue really isn't necessary. */
  {
    #if DEBUG
      unsigned int level;

      for( level=0; level < NUM_RUN_QUEUES; level++ )
      {
        if( isInQueue( &runQueues[level], thread ) )
          kprintf("Thread is in run queue, but it's paused?\n");
        assert( !isInQueue( &runQueues[level], thread ) );
      }
    #endif /* DEBUG */

    thread->threadState = READY;

    attachRunQueue( thread );
    return 0;
  }
  else
  {
    kprintf("Can't start a thread unless it's paused!\n");
    return 1;
  }
}


/**
    Temporarily pauses a thread for an amount of time.

    @param thread The TCB of the thread to put to sleep.
    @param msecs The amount of time to pause the thread in milliseconds.
    @return 0 on success. -1 on failure. -2 if the thread cannot be switched
            to the sleeping state from its current state. 1 if the thread is
            already sleeping.
*/

int sleepThread( TCB *thread, unsigned int msecs )
{
  if( thread->threadState == SLEEPING )
    RET_MSG(1, "Already sleeping!")//return -1;
  else if( msecs >= (1u << 16) )
    RET_MSG(1, "Invalid sleep interval");

  if( thread->threadState != READY && thread->threadState != RUNNING )
  {
    kprintf("sleepThread() failed!\n");
    assert(false);
    return -2;
  }

  if( thread->threadState == READY && thread != idleThread )
    detachRunQueue( thread );

  if( !timerEnqueue( thread, (msecs * HZ) / 1000 ) )
  {
    assert( false );
    return -1;
  }
  else
  {
    thread->reschedule = 1;
    thread->threadState = SLEEPING;
    return 0;
  }
}

/// Pauses a thread

int pauseThread( TCB *thread )
{
  switch( thread->threadState )
  {
    case READY:
      detachRunQueue( thread );
    case RUNNING:
      thread->threadState = PAUSED;
      thread->reschedule = 1;
      return 0;
    case PAUSED:
      RET_MSG(1, "Thread is already paused.")//return 1;
    default:
      kprintf("Thread state: %d\n", thread->threadState);
      RET_MSG(-1, "Thread is neither ready, running, nor paused.")//return -1;
  }
}

/**
    Creates and initializes a new thread.

    @param threadAddr The start address of the thread.
    @param addrSpace The physical address of the thread's page directory.
    @param -tack The address of the top of the thread's stack.
    @param exHandler The TID of the thread's exception handler.
    @return The TCB of the newly created thread. NULL on failure.
*/

TCB *createThread( addr_t threadAddr, addr_t addrSpace, addr_t stack, TCB *exHandler )
{
  TCB * thread = NULL;
  pde_t pde;

  #if DEBUG
    if( exHandler == NULL )
      kprintf("createThread(): NULL exception handler.\n");
  #endif /* DEBUG */

  if( threadAddr == NULL )
    RET_MSG(NULL, "NULL thread addr")
  else if( addrSpace == NULL_PADDR )
    RET_MSG(NULL, "NULL addrSpace")

  assert( (addrSpace & 0xFFFu) == 0u );

  thread = popQueue(&freeThreadQueue);

  if( thread == NULL )
    RET_MSG(NULL, "Couldn't get a free thread.")

  assert( thread->threadState == DEAD );

  thread->priority = NORMAL_PRIORITY;
  thread->cr3.base = (addrSpace >> 12);
  thread->cr3.pwt = 1;
  thread->exHandler = exHandler;
  thread->waitThread = NULL;
  thread->threadQueue.tail = thread->threadQueue.head = NULL;
  thread->queueNext = thread->queuePrev = thread->timerNext = NULL;
  thread->timerDelta = 0u;
  thread->sig_handler = NULL;

  assert( (u32)addrSpace == ((u32)addrSpace & ~0xFFFu) );

   // Map the page directory into the address space

  *(u32 *)&pde = (u32)addrSpace | PAGING_PWT | PAGING_RW | PAGING_PRES;
  writePDE( PAGETAB, &pde, addrSpace );

  // Map the kernel into the address space

  readPDE( KERNEL_VSTART, &pde, (getCR3() & ~0xFFFu) );
  writePDE( KERNEL_VSTART, &pde, addrSpace );

#if DEBUG
  readPDE( 0x00, &pde, (getCR3() & ~0xFFFu) );
  writePDE( 0x00, &pde, addrSpace );
#endif /* DEBUG */

  memset(&thread->execState.user, 0, sizeof thread->execState.user);

  thread->execState.user.eflags = 0x0201u; //0x3201u; // XXX: Warning: Magic Number
  thread->execState.user.eip = ( dword ) threadAddr;

  /* XXX: This doesn't yet initialize a kernel thread other than the idle thread. */

  if( GET_TID(thread) == IDLE_TID )
  {
    thread->execState.user.cs = KCODE;
    thread->execState.user.ds = thread->execState.user.es = KDATA;
    thread->execState.user.esp = stack - (sizeof(ExecutionState)-8);

    memcpy( (void *)thread->execState.user.esp,
            (void *)&thread->execState, sizeof(ExecutionState) - 8);
  }
  else if( ((unsigned int)threadAddr & KERNEL_VSTART) != KERNEL_VSTART )
  {
    thread->execState.user.cs = UCODE;
    thread->execState.user.ds = thread->execState.user.es = thread->execState.user.userSS = UDATA;
    thread->execState.user.userEsp = stack;
  }

  thread->threadState = PAUSED;

  return thread;
}

/**
    Destroys a thread and detaches it from all queues.

    @param thread The TCB of the thread to release.
    @return 0 on success. -1 on failure.
*/

int releaseThread( TCB *thread )
{
  #if DEBUG
    TCB *retThread;
  #endif

  assert(thread->threadState != DEAD);

  if( thread->threadState == DEAD )
    return -1;

  /* If the thread is a sender waiting for a recipient, remove the
     sender from the recipient's wait queue. If the thread is a
     receiver, clear its wait queue after
     waking all of the waiting threads. */

  switch( thread->threadState )
  {
    /* Assumes that a ready/running thread can't be on the timer queue. */

    case READY:
      #if DEBUG
        retThread =
      #endif /* DEBUG */

      detachQueue( &runQueues[thread->priority], thread );

      assert( retThread == thread );
      break;
    case WAIT_FOR_RECV:
    case WAIT_FOR_SEND:
    case SLEEPING:
      #if DEBUG
       retThread =
      #endif /* DEBUG */

      timerDetach( thread );

      assert( retThread == thread );
      break;
  }

  /* XXX: Are these implemented properly? */

  if( thread->threadState == WAIT_FOR_SEND )
  {
    TCB *waitingThread;

    while( (waitingThread=popQueue( &thread->threadQueue )) != NULL )
    {
      waitingThread->threadState = READY;
      attachRunQueue(waitingThread);
    }
  }
  else if( thread->threadState == WAIT_FOR_RECV )
  {
    assert( thread->waitThread != NULL ); // XXX: Why?

    detachQueue( &thread->waitThread->threadQueue, thread );
  }

  memset(thread, 0, sizeof *thread);
  thread->threadState = DEAD;

  enqueue(&freeThreadQueue, thread);
  return 0;
}
