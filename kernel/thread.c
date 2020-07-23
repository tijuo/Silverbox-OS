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

tcb_t *init_server;
tcb_t *currentThread;
tree_t tcbTree;
static tid_t lastTID=0;
static tid_t getNewTID(void);

//extern void saveAndSwitchContext( tcb_t *, tcb_t * );

/** Starts a non-running thread

    The thread is started by placing it on a run queue.

    @param thread The TCB of the thread to be started.
    @return 0 on success. -1 on failure. 1 if the thread doesn't need to be started.
*/
int startThread( tcb_t *thread )
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
      return E_FAIL;
    default:
      break;
  }

  thread->threadState = READY;
  thread->waitPort = NULL_PID;

  attachRunQueue( thread );
  return E_OK;
}

/**
    Temporarily pauses a thread for an amount of time.

    @param thread The TCB of the thread to put to sleep.
    @param msecs The amount of time to pause the thread in milliseconds.
    @return 0 on success. -1 on invalid arguments. -2 if the thread
            cannot be switched to the sleeping state from its current
            state. 1 if the thread is already sleeping.
*/

int sleepThread( tcb_t *thread, int msecs )
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

  if( thread->threadState == READY )
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

int pauseThread( tcb_t *thread )
{
  switch( thread->threadState )
  {
    case READY:
      detachRunQueue( thread );
      thread->threadState = PAUSED;
      return 0;
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

tcb_t *createThread( addr_t threadAddr, paddr_t addrSpace, addr_t stack, pid_t exHandler )
{
  tcb_t * thread = NULL;
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

  thread = malloc(sizeof(tcb_t));

  if( thread == NULL )
    RET_MSG(NULL, "Couldn't allocate memory for a thread.")

  thread->tid = getNewTID();
  thread->priority = NORMAL_PRIORITY;

  thread->cr3 = (dword)addrSpace;
  thread->exHandler = exHandler;

  thread->waitPort = NULL_PID;
  thread->queue.next = NULL_TID;
  thread->queue.delta = 0u;

  memset(&thread->execState.user, 0, sizeof thread->execState.user);

  thread->execState.user.eflags = EFLAGS_IOPL3 | EFLAGS_IF;
  thread->execState.user.eip = ( dword ) threadAddr;

  thread->execState.user.cs = UCODE;
  thread->execState.user.userEsp = stack;
  thread->execState.user.userSS = UDATA;

  thread->threadState = PAUSED;

  if(tree_insert(&tcbTree, thread->tid, thread) != E_OK)
  {
    free(thread);
    return NULL;
  }

   // Map the page directory, kernel space, and first page table
   // into the new address space

  pentry = (u32)addrSpace | PAGING_RW | PAGING_PRES;

  if(unlikely(writePmapEntry(addrSpace, PDE_INDEX(PAGETAB), &pentry) != E_OK))
  {
    tree_remove(&tcbTree, thread->tid);
    free(thread);
    return NULL;
  }

  else if(unlikely(writePmapEntry(addrSpace, PDE_INDEX(KERNEL_VSTART), &(((u32 *)PAGEDIR))[PDE_INDEX(KERNEL_VSTART)]) != E_OK))
  {
    tree_remove(&tcbTree, thread->tid);
    free(thread);
    return NULL;
  }

#if DEBUG
  if(unlikely(writePmapEntry(addrSpace, 0, (void *)PAGEDIR) != E_OK))
  {
    tree_remove(&tcbTree, thread->tid);
    free(thread);
    return NULL;
  }
#endif /* DEBUG */

  // Copy any page tables in the kernel region of virtual memory from
  // the bootstrap address space to the thread's

  size_t bufSize = 4*(1023-PDE_INDEX(KERNEL_VSTART));
  void *buf = malloc(bufSize);

  if(buf == NULL)
  {
    tree_remove(&tcbTree, thread->tid);
    free(thread);
    return NULL;
  }

  if(peek(initKrnlPDir, buf, bufSize) != E_OK)
  {
    tree_remove(&tcbTree, thread->tid);
    free(thread);
    free(buf);

    return NULL;
  }

  if(poke(addrSpace, buf, bufSize) != E_OK)
  {
    tree_remove(&tcbTree, thread->tid);
    free(thread);
    free(buf);

    return NULL;
  }

  free(buf);
  return thread;
}

/**
    Destroys a thread and detaches it from all queues.

    @param thread The TCB of the thread to release.
    @return 0 on success. -1 on failure.
*/

int releaseThread( tcb_t *thread )
{
  #if DEBUG
    tcb_t *retThread;
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

  // TODO:
  // Find each port that this thread owns and notify the waiting
  // senders that sending to that port failed.


  const tid_t tid = GET_TID(thread);
/*
  for(port_t *port=getPort((pid_t)1); port != getPort((pid_t)(MAX_PORTS-1)); 
      port++)
  {
    if(port->owner == tid)
    {
      tcb_t *prevWaitTcb;

      for(tcb_t *waitTcb=getTcb(port->sendWaitTail); waitTcb != NULL;
          waitTcb=prevWaitTcb)
      {
        prevWaitTcb = getTcb(waitTcb->queue.prev);

        waitTcb->threadState = READY;
        attachRunQueue(waitTcb);
      }
    }
  }
*/

  if(thread->threadState == WAIT_FOR_RECV && thread->waitPort != NULL_PID)
  {
    port_t *port = getPort(thread->waitPort);

    if(port->sendWaitTail == tid)
      port->sendWaitTail = NULL_TID;
    else
    {
      if(thread->queue.prev != NULL_TID)
        getTcb(thread->queue.prev)->queue.next = thread->queue.next;

      getTcb(thread->queue.next)->queue.prev = thread->queue.prev;
    }
  }

  free(thread);
  return 0;
}

tcb_t *getTcb(tid_t tid)
{
  if(tid == NULL_TID)
    return NULL;
  else
  {
    tcb_t *tcb=NULL;

    if(tree_find(&tcbTree, (int)tid, (void **)&tcb) == E_OK)
      return tcb;
    else
      return NULL;
  }
}

tid_t getNewTID(void)
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
