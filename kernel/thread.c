#include <kernel/debug.h>
#include <string.h>
#include <kernel/thread.h>
#include <kernel/schedule.h>
#include <kernel/mm.h>
#include <util.h>
#include <oslib.h>
#include <kernel/message.h>
#include <kernel/error.h>
#include <kernel/lowlevel.h>
#include <kernel/paging.h>
#include <kernel/interrupt.h>

#define TID_START           1u

ALIGNED(PAGE_SIZE) uint8_t kernelStack[PAGE_SIZE]; // The single kernel stack used by all threads (assumes a uniprocessor system)
uint8_t *kernelStackTop = kernelStack + PAGE_SIZE;

SECTION(".tcb") tcb_t tcbTable[MAX_THREADS];
tcb_t *initServerThread;

list_t pausedList;
list_t zombieList;

static tid_t getNewTid(void);

NON_NULL_PARAMS int wakeupThread(tcb_t *thread) {
  switch(thread->threadState) {
    case RUNNING:
    case READY:
      RET_MSG(E_DONE, "Thread is already awake.");
    case ZOMBIE:
    case INACTIVE:
      RET_MSG(E_FAIL, "Unable to wake up inactive thread.");
    default:
      removeThreadFromList(thread);
      thread->threadState = READY;
      listEnqueue(&runQueues[thread->priority], thread);
      break;
  }

  return E_OK;
}

NON_NULL_PARAMS int removeThreadFromList(tcb_t *thread) {
  switch(thread->threadState) {
    case WAIT_FOR_RECV:
      detachSendWaitQueue(thread);
      thread->waitTid = NULL_TID;
      break;
    case WAIT_FOR_SEND:
      detachReceiveWaitQueue(thread);
      thread->waitTid = NULL_TID;
      break;
    case READY:
      listRemove(&runQueues[thread->priority], thread);
      break;
    case PAUSED:
      listRemove(&pausedList, thread);
      break;
    case ZOMBIE:
      listRemove(&zombieList, thread);
      break;
    case RUNNING:
      for(unsigned int processorId = 0; processorId < MAX_PROCESSORS;
          processorId++)
      {
        if(runningThreads[processorId] == thread) {
          runningThreads[processorId] = NULL;
          break;
        }
      }
      break;
    default:
      RET_MSG(E_FAIL, "Unable to remove thread from list.");
  }

  return E_OK;
}

/** Starts a non-running thread by placing it on a run queue.

 @param thread The thread to be started.
 @return E_OK on success. E_FAIL on failure. E_DONE if the thread is already started.
 */
NON_NULL_PARAMS int startThread(tcb_t *thread) {
  switch(thread->threadState) {
    case WAIT_FOR_RECV:
    case WAIT_FOR_SEND:
    case PAUSED:
      removeThreadFromList(thread);
      break;
    case READY:
      RET_MSG(E_DONE, "Thread has already started.");
    case RUNNING:
      RET_MSG(E_DONE, "Thread is already running.");
    default:
      RET_MSG(E_FAIL, "Unable to start thread.");
  }

  thread->threadState = READY;
  listEnqueue(&runQueues[thread->priority], thread);

  return E_OK;
}

/** Block a thread indefinitely until it is restarted.
 * @param thread The thread to be paused.
 * @return E_OK on success. E_FAIL on failure. E_DONE if thread is already paused.
 */

NON_NULL_PARAMS int pauseThread(tcb_t *thread) {
  switch(thread->threadState) {
    case WAIT_FOR_RECV:
    case WAIT_FOR_SEND:
    case READY:
    case RUNNING:
      removeThreadFromList(thread);
      break;
    case PAUSED:
      RET_MSG(E_DONE, "Thread is already paused.");
    default:
      RET_MSG(E_FAIL, "Thread cannot be paused.");
  }

  thread->threadState = PAUSED;
  listEnqueue(&pausedList, thread);

  return E_OK;
}

/**
 Creates and initializes a new thread.

 @param entryAddr The start address of the thread.
 @param addrSpace The physical address of the thread's page directory.
 @param stackTop The address of the top of the thread's stack.
 @return The TCB of the newly created thread. NULL on failure.
 */

NON_NULL_PARAMS tcb_t* createThread(void *entryAddr, addr_t addrSpace,
                                    void *stackTop)
{
  tcb_t *thread = NULL;
  uint32_t rootPmap =
      addrSpace == CURRENT_ROOT_PMAP ? getRootPageMap() : addrSpace;

  if(IS_ERROR(initializeRootPmap(rootPmap)))
    RET_MSG(NULL, "Unable to initialize page map.");

  tid_t tid = getNewTid();

  if(tid == NULL_TID)
    RET_MSG(NULL, "Unable to create a new thread id.");

  thread = getTcb(tid);

  if(thread->threadState != INACTIVE)
    RET_MSG(NULL, "Thread is already active.");

  memset(thread, 0, sizeof(tcb_t));
  thread->rootPageMap = (dword)rootPmap;

  thread->userExecState.eflags = EFLAGS_IOPL3 | EFLAGS_IF | EFLAGS_RESD;
  thread->userExecState.eip = (dword)entryAddr;

  thread->userExecState.userEsp = (dword)stackTop;
  thread->userExecState.cs = UCODE_SEL;
  thread->userExecState.userSS = UDATA_SEL;

  thread->priority = NORMAL_PRIORITY;

  thread->childrenHead = NULL_TID;

  kprintf("Created new thread at %#p (tid: %u, pmap: %#p)\n", thread, tid,
          (void*)(uintptr_t)thread->rootPageMap);

  thread->threadState = PAUSED;
  listEnqueue(&pausedList, thread);

  tcb_t *currentThread = getCurrentThread();

  if(currentThread) {
    thread->parent = getTid(currentThread);
    thread->nextSibling = currentThread->childrenHead;
    currentThread->childrenHead = tid;
  }
  else {
    thread->parent = NULL_TID;
    thread->nextSibling = NULL_TID;
  }
  return thread;
}

/**
 Destroys a thread and detaches it from all queues.

 @param thread The TCB of the thread to release.
 @return E_OK on success. E_FAIL on failure.
 */

NON_NULL_PARAMS int releaseThread(tcb_t *thread) {
  list_t *queues[2] = {
    &thread->senderWaitQueue,
    &thread->receiverWaitQueue
  };
  list_t *queue;
  size_t i;

  /* If the thread is a sender waiting for a recipient, remove the
   sender from the recipient's wait queue. If the thread is a
   receiver, clear its wait queue after waking all of the waiting
   threads. */

  switch(thread->threadState) {
    case INACTIVE:
      RET_MSG(E_DONE, "Thread is already inactive.");
    default:
      removeThreadFromList(thread);
  }

  /* Notify any threads waiting for a send/receive that the operation
   failed. */

  for(i = 0, queue = queues[0]; i < ARRAY_SIZE(queues); i++, queue++) {
    while(queue->headTid != NULL_TID) {
      tcb_t *t = listDequeue(queue);

      if(t) {
        t->waitTid = NULL_TID;
        t->userExecState.eax = E_UNREACH;
        kprintf("Releasing thread. Starting %u\n", getTid(t));
        t->threadState = READY;
      }
    }
  }

  // Unregister the thread as an exception handler

  for(i = 0; i < NUM_IRQS; i++) {
    if(irqHandlers[i] == thread)
      irqHandlers[i] = NULL_TID;
  }

  // Release any zombie child threads

  for(tid_t tid = thread->childrenHead; tid != NULL_TID;) {
    tcb_t *tcb = getTcb(tid);

    if(tcb->threadState == ZOMBIE)
      releaseThread(tcb);
    else
      tcb->parent = NULL_TID;

    tid = tcb->nextSibling;
  }

  thread->threadState = INACTIVE;
  return E_OK;
}

/**
 * Generate a new TID for a thread.
 *
 * @return A TID that is available for use. NULL_TID if no TIDs are available to be allocated.
 */

tid_t getNewTid(void) {
  static int lastTid = TID_START;
  int prevTid = lastTid++;

  do {
    if(lastTid == MAX_THREADS)
      lastTid = TID_START;
  } while(lastTid != prevTid && getTcb(lastTid)->threadState != INACTIVE);

  if(lastTid == prevTid)
    RET_MSG(NULL_TID, "No more TIDs are available");

  return (tid_t)lastTid;
}

NON_NULL_PARAMS void switchContext(tcb_t *thread, int doXSave) {
  assert(thread->threadState == RUNNING);

  if((thread->rootPageMap & CR3_BASE_MASK) != (getCR3() & CR3_BASE_MASK))
    asm volatile("mov %0, %%cr3\n" :: "r"(thread->rootPageMap));

  tcb_t *currentThread = getCurrentThread();

  if(currentThread) {
    if(doXSave)
      __asm__("fxsave  %0\n" :: "m"(currentThread->xsaveState));
  }

  __asm__("fxrstor %0\n" :: "m"(thread->xsaveState));

//  activateContinuation(thread); // doesn't return if successful

// Restore user state

  tss.esp0 = (uint32_t)((ExecutionState*)kernelStackTop - 1) - sizeof(uint32_t);
  uint32_t *s = (uint32_t*)tss.esp0;
  ExecutionState *state = (ExecutionState*)(s + 1);

  *s = (uint32_t)kernelStackTop;
  *state = thread->userExecState;

  setCR3(initServerThread->rootPageMap);
  RESTORE_STATE;
}
