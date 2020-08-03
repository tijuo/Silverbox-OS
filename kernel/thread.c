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
#include <kernel/lowlevel.h>

#define MAX_ATTEMPTS		1000

tcb_t *initServerThread;
tcb_t *currentThread;
tcb_t *tcbTable=(tcb_t *)&kTcbStart;
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
  if( queueFindFirst(&timerQueue, getTid(thread), NULL) == E_OK )
    kprintf("Thread is in timer queue!\n");
  #endif /* DEBUG */

  assert( queueFindFirst(&timerQueue, getTid(thread), NULL) != E_OK );

  switch(thread->threadState)
  {
    case SLEEPING:
      if(queueRemoveFirst(&timerQueue, getTid(thread), NULL) != E_OK)
        return E_FAIL;
      thread->waitTid = NULL_TID;
      break;
    case WAIT_FOR_RECV:
      if(detachSendQueue(thread) != E_OK)
        return E_FAIL;
      thread->waitTid = NULL_TID;
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

  if(queueEnqueue(&timerQueue, getTid(thread), timerDelta) != E_OK)
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

tcb_t *createThread(addr_t threadAddr, paddr_t addrSpace, addr_t stack)
{
  tcb_t * thread = NULL;
  u32 pentry;
  tid_t tid = getNewTID();

  if( threadAddr == NULL )
    RET_MSG(NULL, "NULL thread addr")
  else if((addrSpace & 0xFFF) != 0)
    RET_MSG(NULL, "Invalid address space address.")
  else if(tid == NULL_TID)
    RET_MSG(NULL, "Unable to create new threads.");

  if(addrSpace == NULL_PADDR)
    addrSpace = getCR3() & ~0x3FF;

  thread = getTcb(tid);

  if( thread == NULL )
    RET_MSG(NULL, "Couldn't allocate memory for a thread.")

  thread->priority = NORMAL_PRIORITY;
  thread->rootPageMap = (dword)addrSpace;
  thread->waitTid = NULL_TID;

  memset(&thread->execState, 0, sizeof thread->execState);

  thread->execState.eflags = EFLAGS_IOPL3 | EFLAGS_IF;
  thread->execState.eip = ( dword ) threadAddr;

  thread->execState.cs = UCODE;
  thread->execState.userEsp = stack;
  thread->execState.userSS = UDATA;

  thread->receiverWaitQueue = malloc(sizeof(queue_t));

  if(!thread->receiverWaitQueue
     || queueInit(thread->receiverWaitQueue) != E_OK)
  {
    return NULL;
  }

  thread->senderWaitQueue = malloc(sizeof(queue_t));

  if(!thread->senderWaitQueue
     || queueInit(thread->senderWaitQueue) != E_OK)
  {
    free(thread->receiverWaitQueue);
    return NULL;
  }

   // Map the page directory, kernel space, and first page table
   // into the new address space

  pentry = (u32)addrSpace | PAGING_RW | PAGING_PRES;

  if(unlikely(writePmapEntry(addrSpace, PDE_INDEX(PAGETAB), &pentry) != E_OK))
  {
    free(thread->senderWaitQueue);
    free(thread->receiverWaitQueue);
    return NULL;
  }
/*
  else if(unlikely(writePmapEntry(addrSpace, PDE_INDEX(KERNEL_VSTART), &(((u32 *)PAGEDIR))[PDE_INDEX(KERNEL_VSTART)]) != E_OK))
  {
    free(thread->senderWaitQueue);
    free(thread->receiverWaitQueue);
    return NULL;
  }
*/
#if DEBUG
  if(unlikely(writePmapEntry(addrSpace, 0, (void *)PAGEDIR) != E_OK))
  {
    free(thread->senderWaitQueue);
    free(thread->receiverWaitQueue);
    return NULL;
  }
#endif /* DEBUG */

  // Copy any page tables in the kernel region of virtual memory from
  // the bootstrap address space to the thread's

  size_t bufSize = 4*(1023-PDE_INDEX(KERNEL_TCB_START));
  void *buf = malloc(bufSize);

  if(!buf)
  {
    free(thread->senderWaitQueue);
    free(thread->receiverWaitQueue);
    return NULL;
  }

  if(peek(initKrnlPDir+4*PDE_INDEX(KERNEL_TCB_START), buf, bufSize) != E_OK)
  {
    free(thread->senderWaitQueue);
    free(thread->receiverWaitQueue);
    free(buf);
    return NULL;
  }

  if(poke(addrSpace+4*PDE_INDEX(KERNEL_TCB_START), buf, bufSize) != E_OK)
  {
    free(thread->senderWaitQueue);
    free(thread->receiverWaitQueue);
    free(buf);
    return NULL;
  }

  free(buf);

  thread->threadState = PAUSED;

  return thread;
}

/**
    Destroys a thread and detaches it from all queues.

    @param thread The TCB of the thread to release.
    @return E_OK on success. E_FAIL on failure.
*/

int releaseThread( tcb_t *thread )
{
  if( thread->threadState == INACTIVE )
    return E_OK;

  /* If the thread is a sender waiting for a recipient, remove the
     sender from the recipient's wait queue. If the thread is a
     receiver, clear its wait queue after
     waking all of the waiting threads. */

  switch( thread->threadState )
  {
    /* Assumes that a ready/running thread can't be on the timer queue. */

    case READY:
      if(queueRemoveFirst(&runQueues[thread->priority], getTid(thread),
                           NULL) != E_OK)
      {
        return E_FAIL;
      }

      break;
    case WAIT_FOR_RECV:
      if(detachSendQueue(thread) != E_OK)
        return E_FAIL;
      break;
    case WAIT_FOR_SEND:
      if(detachReceiveQueue(thread) != E_OK)
        return E_FAIL;
      break;
    case SLEEPING:
      if(queueRemoveFirst(&timerQueue, getTid(thread), NULL) != E_OK)
        return E_FAIL;
      break;
  }

  /* Notify any threads waiting for a send/receive that the operation
     failed. */

  for(tcb_t *t=NULL;
      queuePop(thread->senderWaitQueue, (void *)&t) == E_OK; )
  {
    t->waitTid = NULL_TID;
    t->execState.eax = E_FAIL;
    startThread(t);
  }

  for(tcb_t *t=NULL;
      queuePop(thread->receiverWaitQueue, (void *)&t) == E_OK; )
  {
    t->waitTid = NULL_TID;
    t->execState.eax = E_FAIL;
    startThread(t);
  }

  free(thread->receiverWaitQueue);
  free(thread->senderWaitQueue);

  thread->threadState = INACTIVE;
  return E_OK;
}

tid_t getNewTID(void)
{
  unsigned int i;

  lastTID++;

  for(i=1; (getTcb(i)->threadState != INACTIVE && i < MAX_ATTEMPTS)
      || !lastTID; lastTID += i*i);

  return (i == MAX_ATTEMPTS ? NULL_TID : lastTID);
}
