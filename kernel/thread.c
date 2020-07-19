#include <kernel/debug.h>
#include <string.h>
#include <kernel/thread.h>
#include <kernel/schedule.h>
#include <kernel/mm.h>
#include <kernel/pit.h>
#include <util.h>
#include <oslib.h>
#include <kernel/message.h>
#include <kernel/error.h>
#include <kernel/dlmalloc.h>

TCB *init_server;
TCB *currentThread;
TCB *idleThread;
tree_t tcbTree;
static tid_t lastTID=0;
static tid_t getNewTID();

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

  switch(thread->threadState)
  {
    case SLEEPING:
      timerDetach(thread);
      break;
    case WAIT_FOR_RECV:
      detachSendQueue(thread, thread->waitPort);
      break;
    case WAIT_FOR_SEND:
      detachReceiveQueue(thread, thread->waitPort);
      break;
    case RUNNING:
      return -1;
    default:
      break;
  }

  thread->threadState = READY;
  thread->waitPort = NULL_PID;

  attachRunQueue( thread );
  return 0;
}

/**
    Temporarily pauses a thread for an amount of time.

    @param thread The TCB of the thread to put to sleep.
    @param msecs The amount of time to pause the thread in milliseconds.
    @return 0 on success. -1 on invalid arguments. -2 if the thread
            cannot be switched to the sleeping state from its current
            state. 1 if the thread is already sleeping.
*/

int sleepThread( TCB *thread, int msecs )
{
  if( thread->threadState == SLEEPING )
    RET_MSG(1, "Already sleeping!")//return -1;
  else if( msecs >= (1 << 16) || msecs < 1)
    RET_MSG(-1, "Invalid sleep interval");

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
    @param stack The address of the top of the thread's stack.
    @param exHandler The PID of the thread's exception handler.
    @return The TCB of the newly created thread. NULL on failure.
*/

TCB *createThread( addr_t threadAddr, paddr_t addrSpace, addr_t stack, pid_t exHandler )
{
  TCB * thread = NULL;
  u32 pentry;

  #if DEBUG
    if( exHandler == NULL_TID )
      kprintf("createThread(): NULL exception handler.\n");
  #endif /* DEBUG */

  if( threadAddr == NULL )
    RET_MSG(NULL, "NULL thread addr")
  else if((addrSpace & 0xFFF) != 0)
    RET_MSG(NULL, "Invalid address space address.")

  if(addrSpace == NULL_PADDR)
    addrSpace = getCR3() & 0x3FF;

  thread = malloc(sizeof(TCB));//popQueue(&freeThreadQueue);

  if( thread == NULL )
    RET_MSG(NULL, "Couldn't get a free thread.")

  assert( thread->threadState == DEAD );

  memset(thread, 0, sizeof thread);

  thread->tid = getNewTID();
  thread->priority = NORMAL_PRIORITY;

  thread->cr3 = (dword)addrSpace;
  thread->exHandler = exHandler;

  thread->waitPort = NULL_PID;
  thread->queue.next = NULL_TID;
  thread->queue.delta = 0u;

  memset(&thread->execState.user, 0, sizeof thread->execState.user);

  thread->execState.user.eflags = 0x0201u; //0x3201u; // XXX: Warning: Magic Number
  thread->execState.user.eip = ( dword ) threadAddr;

  /* XXX: This doesn't yet initialize a kernel thread other than the idle thread. */

  if( GET_TID(thread) == IDLE_TID )
  {
    thread->execState.user.cs = KCODE;
    dword *esp = (dword *)(stack - (sizeof(ExecutionState)-8));

    memcpy( (void *)esp,
            (void *)&thread->execState, sizeof(ExecutionState) - 8);
  }
  else if( (threadAddr & KERNEL_VSTART) != KERNEL_VSTART )
  {
    thread->execState.user.cs = UCODE;
    thread->execState.user.userEsp = stack;
  }

  thread->threadState = PAUSED;

  if(tree_insert(&tcbTree, thread->tid, thread) != E_OK)
  {
    free(thread);
    return NULL;
  }

   // Map the page directory and kernel space into the new address space

  pentry = (u32)addrSpace | PAGING_RW | PAGING_PRES;

  if(unlikely(writePmapEntry(addrSpace, PDE_INDEX(PAGETAB), &pentry) != 0))
  {
    tree_remove(&tcbTree, thread->tid);
    free(thread);
    return NULL;
  }
  else if(unlikely(writePmapEntry(addrSpace, PDE_INDEX(KERNEL_VSTART), &(((u32 *)PAGEDIR))[PDE_INDEX(KERNEL_VSTART)]) != 0))
  {
    tree_remove(&tcbTree, thread->tid);
    free(thread);
    return NULL;
  }

#if DEBUG
  if(unlikely(writePmapEntry(addrSpace, 0, (void *)PAGEDIR) != 0))
  {
    tree_remove(&tcbTree, thread->tid);
    free(thread);
    return NULL;
  }
#endif /* DEBUG */

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

  // Find each port that this thread owns and notify the waiting
  // senders that sending to that port failed.

  const tid_t tid = GET_TID(thread);

  for(struct Port *port=getPort((pid_t)1); port != getPort((pid_t)(MAX_PORTS-1)); 
      port++)
  {
    if(port->owner == tid)
    {
      TCB *prevWaitTcb;

      for(TCB *waitTcb=getTcb(port->sendWaitTail); waitTcb != NULL;
          waitTcb=prevWaitTcb)
      {
        prevWaitTcb = getTcb(waitTcb->queue.prev);

        waitTcb->threadState = READY;
        attachRunQueue(waitTcb);
      }
    }
  }

  if(thread->threadState == WAIT_FOR_RECV && thread->waitPort != NULL_PID)
  {
    struct Port *port = getPort(thread->waitPort);

    if(port->sendWaitTail == tid)
      port->sendWaitTail = NULL_TID;
    else
    {
      if(thread->queue.prev != NULL_TID)
        getTcb(thread->queue.prev)->queue.next = thread->queue.next;

      getTcb(thread->queue.next)->queue.prev = thread->queue.prev;
    }
  }

/*
  // XXX: Recursive calls may result in stack overflow

  for(TCB *tcbPtr=getTcb(1); tcbPtr != getTcb((tid_t)(MAX_THREADS-1)); tcbPtr++)
  {
    if(tcbPtr->threadState != DEAD && tcbPtr->exHandler == tid)
      releaseThread(tcbPtr);
  }


  memset(thread, 0, sizeof *thread);
  thread->threadState = DEAD;

//  enqueue(&freeThreadQueue, thread);
*/
  free(thread);
  return 0;
}

TCB *getTcb(tid_t tid)
{
  if(tid == NULL_TID)
    return NULL;
  else
  {
    TCB *tcb=NULL;

    if(tree_find(&tcbTree, (int)tid, (void **)&tcb) == E_OK)
      return tcb;
    else
      return NULL;
  }
}

tid_t getNewTID()
{
  int i, maxAttempts=1000;

  lastTID++;

  for(i=1;(tree_find(&tcbTree, (int)lastTID, NULL) == E_OK && i < maxAttempts)
      || !lastTID;
      lastTID += i*i);

  if(i == maxAttempts)
    return NULL_TID;
  else
    return lastTID;
}
