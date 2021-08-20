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
  switch(thread->threadState)
  {
    case SLEEPING:
      if(IS_ERROR(deltaListRemove(&timerQueue, thread)))
        RET_MSG(E_FAIL, "Unable to remove thread from sleep queue.");
      break;
    case WAIT_FOR_RECV:
      if(IS_ERROR(detachSendWaitQueue(thread)))
        RET_MSG(E_FAIL, "Unable to detach thread from sender wait queue.");
      thread->waitTid = NULL_TID;
      break;
    case WAIT_FOR_SEND:
      if(IS_ERROR(detachReceiveWaitQueue(thread)))
        RET_MSG(E_FAIL, "Unable to detach thread from recipient wait queue.");
      thread->waitTid = NULL_TID;
      break;
    case READY:
      RET_MSG(E_DONE, "Thread has already started.");
    case RUNNING:
      RET_MSG(E_DONE, "Thread is already running.");
    default:
      break;
  }

  thread->threadState = READY;

  if(attachRunQueue( thread ))
    return E_OK;
  else
    RET_MSG(E_FAIL, "Unable to attach thread to run queue");
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
    RET_MSG(E_DONE, "Thread is already sleeping.");
  else if(msecs < 0)
    RET_MSG(E_INVALID_ARG, "Invalid sleep interval");

  if( thread->threadState != READY && thread->threadState != RUNNING )
    RET_MSG(E_FAIL, "Thread is neither ready nor running.");

  if( thread->threadState == READY && !detachRunQueue( thread ))
    RET_MSG(E_FAIL, "Unable to detach thread from run queue.");

  if(msecs == 0)
  {
    thread->quantaLeft = 0;
    return E_OK;
  }

  if(IS_ERROR(deltaListPush(&timerQueue, thread, msecs)))
    RET_MSG(E_FAIL, "Unable to add thread to the timer/sleep queue.");
  else
  {
    thread->threadState = SLEEPING;
    return E_OK;
  }
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
        RET_MSG(E_FAIL, "Unable to detach thread from run queue.");
      break;
    case RUNNING:
      break;
    case PAUSED:
      RET_MSG(E_DONE, "Thread is already paused.");
    default:
      RET_MSG(E_FAIL, "Thread is neither ready, running, nor paused.");
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
    RET_MSG(NULL, "NULL entry addr");
  else if(tid == NULL_TID)
    RET_MSG(NULL, "Unable to create new thread.");

  if(rootPmap == NULL_PADDR)
    rootPmap = getRootPageMap();
  else if(IS_ERROR(initializeRootPmap(rootPmap)))
    RET_MSG(NULL, "Unable to initialize page map.");

  thread = getTcb(tid);

  if( thread->threadState != INACTIVE )
    RET_MSG(NULL, "Thread is already active.");

  clearMemory(thread, sizeof(tcb_t));
  thread->priority = NORMAL_PRIORITY;
  thread->rootPageMap = (dword)rootPmap;

  thread->execState.eflags = EFLAGS_IOPL3 | EFLAGS_IF;
  thread->execState.eip = ( dword ) entryAddr;

  thread->execState.cs = UCODE;
  thread->execState.userEsp = thread->execState.ebp = stack;
  thread->execState.userSS = UDATA;

  kprintf("Created new thread at 0x%x (tid: %d, pmap: 0x%x)\n", thread, tid, (u32)thread->rootPageMap);

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
  list_t *queues[2] = { &thread->senderWaitQueue, &thread->receiverWaitQueue };
  list_t *queue;
  size_t i;

  /* If the thread is a sender waiting for a recipient, remove the
     sender from the recipient's wait queue. If the thread is a
     receiver, clear its wait queue after
     waking all of the waiting threads. */

  switch( thread->threadState )
  {
    /* Assumes that a ready/running thread can't be on the timer queue. */

    case READY:
      if(IS_ERROR(listRemove(&runQueues[thread->priority], thread)))
        RET_MSG(E_FAIL, "Unable to release thread from run queue.");
      break;
    case WAIT_FOR_RECV:
      if(IS_ERROR(detachSendWaitQueue(thread)))
        RET_MSG(E_FAIL, "Unable to detach thread from sender wait queue.");
      break;
    case WAIT_FOR_SEND:
      if(IS_ERROR(detachReceiveWaitQueue(thread)))
        RET_MSG(E_FAIL, "Unable to detach thread from recipient wait queue.");
      break;
    case SLEEPING:
      if(IS_ERROR(deltaListRemove(&timerQueue, thread)))
        RET_MSG(E_FAIL, "Unable to remove thread from sleep queue");
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
    while(queue->headTid != NULL_TID)
    {
      tcb_t *t = listDequeue(queue);

      if(t)
      {
        t->waitTid = NULL_TID;
        t->execState.eax = E_FAIL;
        kprintf("Releasing thread. Starting %d\n", getTid(t));
        startThread(t);
      }
    }
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
    RET_MSG(NULL_TID, "No more TIDs are available");

  return (tid_t)lastTid;
}

addr_t allocateKernelStack(void)
{
  for(addr_t stack=(addr_t)EXT_PTR(kernelStacksTop); stack > (addr_t)EXT_PTR(kernelStacksBottom); stack -= KERNEL_STACK_SIZE)
  {
    dword *ptr = (dword *)(stack - sizeof(dword));

    if(!*ptr)
    {
      *ptr = 1;
      return (addr_t)ptr;
    }
  }

  return NULL;
}

void releaseKernelStack(void *stack)
{
  dword ptr = (dword)stack;

  ptr += KERNEL_STACK_SIZE - (ptr & (KERNEL_STACK_SIZE-1));

#if DEBUG
  int found = ptr > (dword)EXT_PTR(kernelStacksBottom) && ptr <= (dword)EXT_PTR(kernelStacksTop);

  if(!found)
    kprintf("Attempted to deallocate invalid kernel stack at 0x%x. 0x%x is <= than 0x%x and > 0x%x\n", (dword)stack, ptr, (dword)EXT_PTR(kernelStacksBottom), (dword)EXT_PTR(kernelStacksTop));
  assert(found);
#endif
  dword *p = (dword *)(ptr-sizeof(dword));
  *p = 0;
}

int saveAndSwitchContext(tcb_t *newThread)
{
  return _saveAndSwitchContext(currentThread, newThread);
}

int _saveAndSwitchContext(tcb_t *oldThread, tcb_t *newThread)
{
  if(!newThread)
    newThread = schedule();

  assert(newThread);

  if(!newThread || oldThread == newThread)
    RET_MSG(E_FAIL, "Switching to the same thread!");

  ExecutionState *oldState = (ExecutionState *)tssEsp0;
  oldState--;

  oldThread->execState = *oldState;

  dword newTss = (dword)allocateKernelStack();

  if(!newTss)
    return -1;

//  kprintf("Switching from %d to %d\n", getTid(oldThread), getTid(newThread));
//  kprintf("saveAndSwitchContext: Saving kernel stack (0x%x) for tid %d\n", oldThread->kernelStack, getTid(oldThread));

  //kprintf("Saving kernel stack at 0x%x for tid %d\n", oldThread->kernelStack, getTid(oldThread));

  tssEsp0 = newTss;

  asm volatile("mov $0, %%eax\n"
	       "pushf\n"
               "push %%eax\n"
               "push %%ecx\n"
               "push %%edx\n"
               "push %%ebx\n"
               "push %%ebp\n"
               "push %%esi\n"
               "push %%edi\n"
               "push %1\n"
               "mov %%esp, %0" : "=m"(oldThread->kernelStack) : "m"(oldThread->kernelStack) : "eax");

  switchContext(newThread);

  return 0;
}

void switchContext(tcb_t *thread)
{
  assert(thread);

  assert(thread == currentThread);
  assert(thread->threadState == RUNNING);

//  kprintf("Switching context...\n");

  if((thread->rootPageMap & CR3_BASE_MASK) != (getCR3() & CR3_BASE_MASK))
    asm volatile("mov %0, %%cr3" :: "r"(thread->rootPageMap));

  if(thread->kernelStack /*&& *((addr_t *)thread->kernelStack)*/)
  {
    addr_t *stack = thread->kernelStack;
    //thread->kernelStack = NULL;
//    addr_t *oldFrame = (addr_t *)*(stack+3);

//    kprintf("switchContext: Loading kernel stack at 0x%x for tid %d\n", stack, getTid(thread));
 //   kprintf("switchContext: Popping previous kernel stack at 0x%x Return address: 0x%x\n", *stack, *(oldFrame+1));

    asm volatile("mov %1, %%esp\n"
                 "pop %0\n" : "=r"(thread->kernelStack) : "m"(stack));
    asm volatile("pop %%edi\n"
                 "pop %%esi\n"
                 "pop %%ebp\n"
                 "pop %%ebx\n"
                 "pop %%edx\n"
                 "pop %%ecx\n"
                 "pop %%eax\n"
                 "popf\n"
                 "leave\n"
                 "ret\n" ::: "eax", "ebx", "ecx", "edx", "esi", "edi");
  }
  else
  {
    ExecutionState *state = (ExecutionState *)tssEsp0;

    assert(state);

    if(!thread->kernelStack && thread->extExecState)
      asm volatile("xrstor (%0)" :: "a"(0xFFFFFFFF), "d"(0xFFFFFFFF), "m"(thread->extExecState));

    if(thread->kernelStack)
      thread->kernelStack = NULL;

    state--;
    *state = thread->execState;

    //kprintf("switchContext: Restoring user state at 0x%x for tid: %d\n", state, getTid(thread));

    asm volatile("mov %1, %%esp\n"
		 "mov %0, %%eax\n"
                 "mov %%eax, %%ds\n"
                 "mov %%eax, %%es\n"
                 "pop %%edi\n"
                 "pop %%esi\n"
                 "pop %%ebp\n"
                 "pop %%ebx\n"
                 "pop %%edx\n"
                 "pop %%ecx\n"
                 "pop %%eax\n"
                 "iret" :: "i"(UDATA), "r"(state) : "eax");
  }
}
