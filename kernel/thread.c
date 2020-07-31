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

tcb_t *initServerThread, *initPagerThread;
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
  if( queueFindFirst(&timerQueue, thread->tid, NULL) == E_OK )
    kprintf("Thread is in timer queue!\n");
  #endif /* DEBUG */

  assert( queueFindFirst(&timerQueue, thread->tid, NULL) != E_OK );

  switch(thread->threadState)
  {
    case SLEEPING:
      if(queueRemoveFirst(&timerQueue, thread->tid, NULL) != E_OK)
        return E_FAIL;
      break;
    case WAIT_FOR_RECV:
      if(detachSendQueue(thread) != E_OK)
        return E_FAIL;
      break;
    case WAIT_FOR_SEND:
      if(detachReceiveQueue(thread) != E_OK)
        return E_FAIL;
      break;
    case RUNNING:
      return E_FAIL;
    default:
      break;
  }

  thread->threadState = READY;
  thread->wait.port = NULL;

  return attachRunQueue( thread ) ? E_OK : E_FAIL;
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
    RET_MSG(E_INVALID_ARG, "Invalid sleep interval");

  if( thread->threadState != READY && thread->threadState != RUNNING )
  {
    kprintf("sleepThread() failed!\n");
    assert(false);
    return E_FAIL;
  }

  if( thread->threadState == READY && detachRunQueue( thread ) != E_OK)
    return E_FAIL;

  timer_delta_t *timerDelta = malloc(sizeof(timer_delta_t));

  if(!timerDelta)
    return E_FAIL;

  timerDelta->delta = (msecs*HZ)/1000;
  timerDelta->thread = thread;

  if(queueEnqueue(&timerQueue, thread->tid, timerDelta) != E_OK)
    return E_FAIL;
  else
  {
    thread->threadState = SLEEPING;
    return E_OK;
  }
}

/// Pauses a thread

int pauseThread( tcb_t *thread )
{
  switch( thread->threadState )
  {
    case READY:
      if(detachRunQueue( thread ) != E_OK)
        return E_FAIL;
      else
      {
        thread->threadState = PAUSED;
        return E_OK;
      }
    case RUNNING:
      thread->threadState = PAUSED;
      return E_OK;
    case PAUSED:
      RET_MSG(1, "Thread is already paused.")//return 1;
    default:
      kprintf("Thread state: %d\n", thread->threadState);
      RET_MSG(E_FAIL, "Thread is neither ready, running, nor paused.")//return -1;
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
    addrSpace = getCR3() & ~0x3FF;

  thread = malloc(sizeof(tcb_t));

  if( thread == NULL )
    RET_MSG(NULL, "Couldn't allocate memory for a thread.")

  thread->tid = getNewTID();
  thread->priority = NORMAL_PRIORITY;

  thread->rootPageMap = (dword)addrSpace;
  thread->exHandler = exHandler;

  thread->wait.port = NULL;

  memset(&thread->execState, 0, sizeof thread->execState);

  thread->execState.eflags = EFLAGS_IOPL3 | EFLAGS_IF;
  thread->execState.eip = ( dword ) threadAddr;

  thread->execState.cs = UCODE;
  thread->execState.userEsp = stack;
  thread->execState.userSS = UDATA;

  thread->threadState = PAUSED;

  if(queueInit(&thread->receiverWaitQueue) != E_OK
     || treeInsert(&tcbTree, thread->tid, thread) != E_OK)
  {
    free(thread);
    return NULL;
  }

   // Map the page directory, kernel space, and first page table
   // into the new address space

  pentry = (u32)addrSpace | PAGING_RW | PAGING_PRES;

  if(unlikely(writePmapEntry(addrSpace, PDE_INDEX(PAGETAB), &pentry) != E_OK))
  {
    treeRemove(&tcbTree, thread->tid, NULL);
    free(thread);
    return NULL;
  }

  else if(unlikely(writePmapEntry(addrSpace, PDE_INDEX(KERNEL_VSTART), &(((u32 *)PAGEDIR))[PDE_INDEX(KERNEL_VSTART)]) != E_OK))
  {
    treeRemove(&tcbTree, thread->tid, NULL);
    free(thread);
    return NULL;
  }

#if DEBUG
  if(unlikely(writePmapEntry(addrSpace, 0, (void *)PAGEDIR) != E_OK))
  {
    treeRemove(&tcbTree, thread->tid, NULL);
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
    treeRemove(&tcbTree, thread->tid, NULL);
    free(thread);
    return NULL;
  }

  if(peek(initKrnlPDir, buf, bufSize) != E_OK)
  {
    treeRemove(&tcbTree, thread->tid, NULL);
    free(thread);
    free(buf);

    return NULL;
  }

  if(poke(addrSpace, buf, bufSize) != E_OK)
  {
    treeRemove(&tcbTree, thread->tid, NULL);
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

  assert(thread->threadState != STOPPED);

  if( thread->threadState == STOPPED )
    return -1;

  /* If the thread is a sender waiting for a recipient, remove the
     sender from the recipient's wait queue. If the thread is a
     receiver, clear its wait queue after
     waking all of the waiting threads. */

  switch( thread->threadState )
  {
    /* Assumes that a ready/running thread can't be on the timer queue. */

    case READY:
      if(queueRemoveFirst( &runQueues[thread->priority], thread->tid,
        #if DEBUG
          (void **)retThread
        #else
          NULL
        #endif
      ) != E_OK)
      {
        return E_FAIL;
      }

      assert( retThread == thread );
      break;
    case WAIT_FOR_RECV:
    case WAIT_FOR_SEND:
    case SLEEPING:
      if(queueRemoveFirst(&timerQueue, thread->tid,
        #if DEBUG
          (void **)&retThread
        #else
          NULL
        #endif
      ) != E_OK)
      {
        return E_FAIL;
      }

      assert( retThread == thread );
      break;
  }

  // TODO:
  // Find each port that this thread owns and notify the waiting
  // senders that sending to that port failed.

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

/*
  if(thread->threadState == WAIT_FOR_RECV && thread->wait.port != NULL_PID)
  {
    port_t *port = getPort(thread->wait.port);

    if(port->sendWaitTail == thread->tid)
      port->sendWaitTail = NULL_TID;
    else
    {
      if(thread->queue.prev != NULL_TID)
        getTcb(thread->queue.prev)->queue.next = thread->queue.next;

      getTcb(thread->queue.next)->queue.prev = thread->queue.prev;
    }
  }
*/
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

    if(treeFind(&tcbTree, (int)tid, (void **)&tcb) == E_OK)
      return tcb;
    else
      return NULL;
  }
}

tid_t getNewTID(void)
{
  int i, maxAttempts=1000;

  lastTID++;

  for(i=1;(treeFind(&tcbTree, (int)lastTID, NULL) == E_OK && i < maxAttempts)
      || !lastTID;
      lastTID += i*i);

  if(i == maxAttempts)
    return NULL_TID;
  else
    return lastTID;
}
