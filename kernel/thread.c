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

#define MSECS_PER_SEC       1000u

tcb_t *initServerThread;
tcb_t *currentThread;
tcb_t *tcbTable=(tcb_t *)&kTcbStart;
static int lastTid=GET_TID_START;
static tid_t getNewTid(void);

/** Starts a non-running thread by placing it on a run queue.

    @param thread The thread to be started.
    @return E_OK on success. E_FAIL on failure. E_DONE if the thread is already started.
*/
int startThread( tcb_t *thread )
{
  #if DEBUG
  if( !IS_ERROR(queueFindFirst(&timerQueue, getTid(thread), NULL)) )
    kprintf("Thread is in timer queue!\n");
  #endif /* DEBUG */

  assert( IS_ERROR(queueFindFirst(&timerQueue, getTid(thread), NULL)) );

  switch(thread->threadState)
  {
    case SLEEPING:
      if(IS_ERROR(queueRemoveFirst(&timerQueue, getTid(thread), NULL)))
        RET_MSG(E_FAIL, "Unable to remove thread from sleep queue.")
      break;
    case WAIT_FOR_RECV:
      if(IS_ERROR(detachSendWaitQueue(thread)))
        RET_MSG(E_FAIL, "Unable to detach thread from sender wait queue.")
      thread->waitTid = NULL_TID;
      break;
    case WAIT_FOR_SEND:
      if(IS_ERROR(detachReceiveWaitQueue(thread)))
        RET_MSG(E_FAIL, "Unable to detach thread from recipient wait queue.")
      thread->waitTid = NULL_TID;
      break;
    case READY:
      RET_MSG(E_DONE, "Thread has already started.")
    case RUNNING:
      RET_MSG(E_DONE, "Thread is already running.")
    default:
      break;
  }

  thread->threadState = READY;

  if(attachRunQueue( thread ))
    return E_OK;
  else
    RET_MSG(E_FAIL, "Unable to attach thread to run queue")
}

/**
    Temporarily blocks execution of a thread for a set amount of time.

    @param thread The thread to put to sleep.
    @param msecs The amount of time to pause the thread in milliseconds. 0
                 if the thread is intended to be placed back into the run queue.
    @return E_OK on success. E_INVALID_ARG on invalid arguments. E_FAIL if the thread
            cannot be switched to the sleeping state from its current
            state. E_DONE if the thread is already sleeping.
*/

int sleepThread( tcb_t *thread, int msecs )
{
  if( thread->threadState == SLEEPING )
    RET_MSG(E_DONE, "Thread is already sleeping.")
  else if(msecs < 0)
    RET_MSG(E_INVALID_ARG, "Invalid sleep interval");

  if( thread->threadState != READY && thread->threadState != RUNNING )
    RET_MSG(E_FAIL, "Thread is neither ready nor running.")

  if( thread->threadState == READY && !detachRunQueue( thread ))
    RET_MSG(E_FAIL, "Unable to detach thread from run queue.")

  if(msecs == 0)
  {
    thread->quantaLeft = 0;
    return E_OK;
  }

  timer_delta_t *timerDelta = kmalloc(sizeof(timer_delta_t));

  if(!timerDelta)
    RET_MSG(E_FAIL, "Unable to allocate memory for timer delta struct.")

  list_node_t *newNode = kmalloc(sizeof(list_node_t));

  if(!newNode)
  {
    kfree(timerDelta, sizeof(timerDelta));
    RET_MSG(E_FAIL, "Unable to allocate memory for new list node")
  }

  newNode->elem = (void *)timerDelta;
  newNode->key = getTid(thread);
  newNode->prev = NULL;
  newNode->next = NULL;

  timerDelta->delta = (msecs*TIMER_QUANTA_HZ) / MSECS_PER_SEC;
  timerDelta->thread = thread;

  if(timerQueue.head)
  {
    for(list_node_t *node=timerQueue.head; node; node = node->next)
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

/** Block a thread indefinitely until it is restarted.
 * @param thread The thread to be paused.
 * @return E_OK on success. E_FAIL on failure. E_DONE if thread is already paused.
 */

int pauseThread( tcb_t *thread )
{
  switch( thread->threadState )
  {
    case READY:
      if(!detachRunQueue( thread ))
        RET_MSG(E_FAIL, "Unable to detach thread from run queue.")
      break;
    case RUNNING:
      break;
    case PAUSED:
      RET_MSG(E_DONE, "Thread is already paused.")
    default:
      kprintf("Thread state: %d\n", thread->threadState);
      RET_MSG(E_FAIL, "Thread is neither ready, running, nor paused.")
  }

  thread->threadState = PAUSED;
  return E_OK;
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
  else if(tid == NULL_TID)
    RET_MSG(NULL, "Unable to create new thread.");

  if(rootPmap == NULL_PADDR)
    rootPmap = getRootPageMap();
  else if(IS_ERROR(initializeRootPmap(rootPmap)))
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
  queue_t *queues[2] = { thread->senderWaitQueue, thread->receiverWaitQueue };
  queue_t *queue;
  size_t i;

  kprintf("Releasing thread 0x%x\n", thread);

  /* If the thread is a sender waiting for a recipient, remove the
     sender from the recipient's wait queue. If the thread is a
     receiver, clear its wait queue after
     waking all of the waiting threads. */

  switch( thread->threadState )
  {
    /* Assumes that a ready/running thread can't be on the timer queue. */

    case READY:
      if(IS_ERROR(queueRemoveFirst(&runQueues[thread->priority], getTid(thread), NULL)))
        RET_MSG(E_FAIL, "Unable to release thread from run queue.")
      break;
    case WAIT_FOR_RECV:
      if(IS_ERROR(detachSendWaitQueue(thread)))
        RET_MSG(E_FAIL, "Unable to detach thread from sender wait queue.")
      break;
    case WAIT_FOR_SEND:
      if(IS_ERROR(detachReceiveWaitQueue(thread)))
        RET_MSG(E_FAIL, "Unable to detach thread from recipient wait queue.")
      break;
    case SLEEPING:
      if(IS_ERROR(queueRemoveFirst(&timerQueue, getTid(thread), NULL)))
        RET_MSG(E_FAIL, "Unable to remove thread from sleep queue")
      break;
    case INACTIVE:
      RET_MSG(E_DONE, "Thread is already inactive.");
    default:
      break;
  }

  /* Notify any threads waiting for a send/receive that the operation
     failed. */

  for(i=0, queue=queues[0]; i < ARRAY_SIZE(queues); i++, queue++)
  {
    for(tcb_t *t=NULL; !IS_ERROR(queuePop(queue, (void *)&t)); )
    {
      t->waitTid = NULL_TID;
      t->execState.eax = E_FAIL;
      startThread(t);
    }

    queueDestroy(queue);
    kfree(queue, sizeof *queue);
  }

  thread->threadState = INACTIVE;
  return E_OK;
}

/**
 * Generate a new TID for a thread.
 *
 * @return A TID that is available for use. NULL_TID if no TIDs are available to be allocated.
 */

tid_t getNewTid(void)
{
  int prevTid=lastTid++;

  do
  {
    if(lastTid == MAX_THREADS)
      lastTid = GET_TID_START;
  } while(lastTid != prevTid && getTcb(lastTid)->threadState != INACTIVE);

  if(lastTid == prevTid)
    RET_MSG(NULL_TID, "No more TIDs are available")

  return (tid_t)lastTid;
}
