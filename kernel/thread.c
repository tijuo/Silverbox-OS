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
#include <kernel/kmalloc.h>
#include <kernel/lowlevel.h>
#include <kernel/paging.h>

tcb_t *initServerThread;
tcb_t *currentThread;
tcb_t *tcbTable=(tcb_t *)&kTcbStart;
static int lastTid=GET_TID_START;
static tid_t getNewTid(void);

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
      break;
    case WAIT_FOR_RECV:
      if(detachSendWaitQueue(thread) != E_OK)
        return E_FAIL;
      thread->waitTid = NULL_TID;
      break;
    case WAIT_FOR_SEND:
      if(detachReceiveWaitQueue(thread) != E_OK)
        return E_FAIL;
      thread->waitTid = NULL_TID;
      break;
    case READY:
      return E_DONE;
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
    return E_DONE;
  else if( msecs >= (1 << 16) || msecs < 1)
    RET_MSG(E_INVALID_ARG, "Invalid sleep interval");

  if( thread->threadState != READY && thread->threadState != RUNNING )
  {
    kprintf("sleepThread() failed!\n");
    assert(false);
    return E_FAIL;
  }

  if( thread->threadState == READY && !detachRunQueue( thread ))
    return E_FAIL;

  if(msecs == 0)
  {
    thread->quantaLeft = 0;
    return E_OK;
  }

  timer_delta_t *timerDelta = kmalloc(sizeof(timer_delta_t));

  if(!timerDelta)
    return E_FAIL;

  list_node_t *newNode = kmalloc(sizeof(list_node_t));

  if(!newNode)
  {
    kfree(timerDelta, sizeof(timerDelta));
    return E_FAIL;
  }

  newNode->elem = (void *)timerDelta;
  newNode->key = getTid(thread);
  newNode->prev = newNode->next = NULL;

  timerDelta->delta = (msecs*HZ) / 1000;
  timerDelta->thread = thread;

  if(timerQueue.head)
  {
    for(list_node_t *node=timerQueue.head; node != NULL; node = node->next)
    {
      timer_delta_t *nodeDelta = (timer_delta_t *)node->elem;

      if(timerDelta->delta > nodeDelta->delta)
        timerDelta->delta -= nodeDelta->delta;
      else
      {
        nodeDelta->delta -= timerDelta->delta;

        if(node->prev)
          node->prev->next = newNode;
        else
          timerQueue.head = newNode;

        newNode->next = node;
        node->prev = newNode;
        goto finish;
      }
    }

    timerQueue.tail->next = newNode;
    newNode->prev = timerQueue.tail;
    timerQueue.tail = newNode;
  }
  else
    timerQueue.head = timerQueue.tail = newNode;

finish:
  thread->threadState = SLEEPING;
  return E_OK;
}

/// Pauses a thread

int pauseThread( tcb_t *thread )
{
  switch( thread->threadState )
  {
    case READY:
      if(!detachRunQueue( thread ))
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

    @param entryAddr The start address of the thread.
    @param rootPmap The physical address of the thread's page directory.
    @param stack The address of the top of the thread's stack.
    @param exHandler The PID of the thread's exception handler.
    @return The TCB of the newly created thread. NULL on failure.
*/

tcb_t *createThread(tid_t desiredTid, addr_t entryAddr, paddr_t rootPmap, addr_t stack)
{
  tcb_t * thread = NULL;
  tid_t tid = (desiredTid == NULL_TID ? getNewTid() : desiredTid);

  if(!entryAddr)
    RET_MSG(NULL, "NULL entry addr")
  else if((rootPmap & 0xFFF) != 0)
    RET_MSG(NULL, "Invalid root page map address.")
  else if(tid == NULL_TID)
    RET_MSG(NULL, "Unable to create new thread.");

  if(rootPmap == NULL_PADDR)
    rootPmap = getCR3() & ~0x3FF;
  else if(initializeRootPmap(rootPmap) != E_OK)
    RET_MSG(NULL, "Unable to initialize page map.");

  thread = getTcb(tid);

  if( thread->threadState != INACTIVE )
    RET_MSG(NULL, "Thread is already active.")

  thread->priority = NORMAL_PRIORITY;
  thread->rootPageMap = (dword)rootPmap;
  thread->waitTid = NULL_TID;

  memset(&thread->execState, 0, sizeof thread->execState);

  thread->execState.eflags = EFLAGS_IOPL3 | EFLAGS_IF;
  thread->execState.eip = ( dword ) entryAddr;

  thread->execState.cs = UCODE;
  thread->execState.userEsp = stack;
  thread->execState.userSS = UDATA;

  thread->receiverWaitQueue = kmalloc(sizeof(queue_t));
  thread->senderWaitQueue = kmalloc(sizeof(queue_t));

  if(!thread->receiverWaitQueue
     || queueInit(thread->receiverWaitQueue) != E_OK
     || !thread->senderWaitQueue
     || queueInit(thread->senderWaitQueue) != E_OK)
  {
    if(thread->receiverWaitQueue)
    {
      queueDestroy(thread->receiverWaitQueue);
      kfree(thread->receiverWaitQueue, sizeof *thread->receiverWaitQueue );
    }

    if(thread->senderWaitQueue)
    {
      queueDestroy(thread->senderWaitQueue);
      kfree(thread->senderWaitQueue, sizeof *thread->senderWaitQueue);
    }

    RET_MSG(NULL, "Unable to initialize message queues.");
  }

  kprintf("Created new thread at 0x%x\n", thread);

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
  kprintf("Releasing thread 0x%x\n", thread);

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
      if(detachSendWaitQueue(thread) != E_OK)
        return E_FAIL;
      break;
    case WAIT_FOR_SEND:
      if(detachReceiveWaitQueue(thread) != E_OK)
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

  queueDestroy(thread->receiverWaitQueue);
  queueDestroy(thread->senderWaitQueue);

  kfree(thread->receiverWaitQueue, sizeof *thread->receiverWaitQueue);
  kfree(thread->senderWaitQueue, sizeof *thread->senderWaitQueue);

  thread->threadState = INACTIVE;
  return E_OK;
}

tid_t getNewTid(void)
{
  int prevTid=lastTid++;

  do
  {
    if(lastTid == MAX_THREADS)
      lastTid = GET_TID_START;
  } while(lastTid != prevTid && getTcb(lastTid)->threadState != INACTIVE);

  return (tid_t)(lastTid == prevTid ? NULL_TID : lastTid);
}
