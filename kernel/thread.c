#include <kernel/debug.h>
#include <oslib.h>
#include <string.h>
#include <kernel/thread.h>
#include <kernel/schedule.h>
#include <kernel/mm.h>

#define DEFAULT_USTACK 0xCAFED00D

//extern TCB *idleThread;

//extern void saveAndSwitchContext( volatile TCB * volatile, volatile TCB * volatile );

/** Returns an unused TID

    @return A free, unused TID.
*/

tid_t getFreeTID(void)
{
  static tid_t lastTID = INITIAL_TID;
  int count = 0;

  while( tcbTable[lastTID].state != DEAD && count < maxThreads )
  {
    lastTID = (lastTID == maxThreads - 1 ? INITIAL_TID + 1 : lastTID + 1);
    count++;
  }

  if( tcbTable[lastTID].state == DEAD )
    return lastTID;
  else
    RET_MSG(NULL_TID, "Exhausted all TIDs!");
}

/** Starts a non-running thread

    The thread is started by placing it on a run queue.

    @param thread The TCB of the thread to be started.
    @return 0 on success. -1 on failure. 1 if the thread doesn't need to be started.
*/
int startThread( TCB *thread )
{
  if( isInTimerQueue(GET_TID(thread)) )
    kprintf("Thread is in timer queue!\n");

  assert( !isInTimerQueue(GET_TID(thread)) );

  if( thread->state == PAUSED ) /* A paused queue really isn't necessary. */
  {
    for( int level=0; level < maxRunQueues; level++ )
    {
      if( isInQueue( &runQueues[level], GET_TID(thread) ) )
        kprintf("Thread is in run queue, but it's paused?\n");
      assert( !isInQueue( &runQueues[level], GET_TID(thread) ) );
    }

    assert( thread != currentThread ); // A paused thread should NEVER be running

    thread->state = READY;
    thread->quantaLeft = thread->priority + 1;

    attachRunQueue( thread );
    return 0;
  }
  else
    return 1;
}


/**
    Temporarily pauses a thread for an amount of time.

    @param thread The TCB of the thread to put to sleep.
    @param msecs The amount of time to pause the thread in milliseconds.
    @return 0 on success. -1 on failure. -2 if the thread cannot be switched
            to the sleeping state from its current state. 1 if the thread is 
            already sleeping.
*/

int sleepThread( TCB *thread, int msecs )
{
  assert( msecs > 0 );

  if( thread->state == SLEEPING )
    RET_MSG(1, "Already sleeping!")//return -1;
  else if( msecs <= 0 )
    RET_MSG(1, "Invalid sleep interval");

  if( thread->state != READY && thread->state != RUNNING )
  {
    kprintf("sleepThread() failed!\n");
    assert(false);
    return -2;
  }

  if( thread->state == READY && GET_TID(thread) != IDLE_TID )
    detachRunQueue( thread );

  if( timerEnqueue( GET_TID(thread), (msecs * HZ) / 1000 ) != 0 )
  {
    assert( false );
    return -1;
  }
  else
  {
    assert( thread == &tcbTable[GET_TID(thread)] );
    thread->quantaLeft = 0;
    thread->state = SLEEPING;
    return 0;
  }
}

/// Pauses a thread

int pauseThread( TCB *thread )
{
  switch( thread->state )
  {
    case READY:
      detachRunQueue( thread );
    case RUNNING:
      //attachPausedQueue( thread ); /* A paused queue isn't necessary */
      thread->state = PAUSED;
      thread->quantaLeft = 0;
      return 0;
    case PAUSED:
      RET_MSG(1, "Already Paused")//return 1;
    default:
      RET_MSG(-1, "Paused from other state")//return -1;
  }
}

/**
    Creates and initializes a new thread.

    @param threadAddr The start address of the thread.
    @param addrSpace The physical address of the thread's page directory.
    @param uStack The address of the top of the thread's user stack.
    @param exHandler The TID of the thread's exception handler.
    @return The TCB of the newly created thread. NULL on failure.
*/

TCB *createThread( addr_t threadAddr, addr_t addrSpace, addr_t uStack, tid_t exHandler )
{
   TCB * thread;
   tid_t tid = NULL_TID;
   pde_t pde;
   pte_t pte;

   if( threadAddr == NULL )
     RET_MSG(NULL, "NULL thread addr")
   else if( exHandler == NULL_TID )
     RET_MSG(NULL, "NULL exHandler")
   else if( addrSpace == NULL_PADDR )
     RET_MSG(NULL, "NULL addrSpace")

   tid = getFreeTID();

   if( tid == NULL_TID )
     RET_MSG(NULL, "NULL tid")

    thread = &tcbTable[tid];

    thread->priority = NORMAL_PRIORITY;
    thread->state = PAUSED;
    thread->addrSpace = addrSpace;
    thread->exHandler = exHandler;
    thread->wait_tid = NULL_TID;
    thread->threadQueue.tail = thread->threadQueue.head = NULL_TID;
    thread->sig_handler = NULL;

    assert( thread->addrSpace == tcbTable[tid].addrSpace );
    assert( (u32)addrSpace == ((u32)addrSpace & ~0xFFF) );

    *(u32 *)&pde = (u32)addrSpace | PAGING_RW | PAGING_PRES;
    writePDE( (void *)PAGETAB, &pde, addrSpace );

    readPDE( (void *)KERNEL_VSTART, &pde, (void *)getCR3() );
    writePDE( (void *)KERNEL_VSTART, &pde, addrSpace );

    readPDE( (void *)(KERNEL_VSTART + TABLE_SIZE), &pde, (void *)getCR3() );
    writePDE( (void *)(KERNEL_VSTART + TABLE_SIZE), &pde, addrSpace );

    if( tid == 0 )
    {
      *(u32 *)&pde = (u32)FIRST_PAGE_TAB | PAGING_RW | PAGING_PRES;
      writePDE( (void *)0, &pde, addrSpace );
    }
    else if( tid == 1 )
    {
      *(u32 *)&pde = (u32)INIT_FIRST_PAGE_TAB | PAGING_RW | PAGING_PRES | PAGING_USER;
      writePDE( (void *)0, &pde, addrSpace );
    }

    // Assume that the first page table is already mapped

    #if DEBUG
      pde.present = 0;
      readPDE((void *)0x0, &pde, addrSpace);
      assert(pde.present);
    #endif

    for( unsigned addr=0; addr <= 0xc5000; addr += 0x1000 )
    {
      // Skip the io perm bitmap
      if( addr >= 0xC0000 && addr < 0xC2000 )
        continue;

      readPTE( (void *)addr, &pte, (void *)getCR3() );
      writePTE( (void *)addr, &pte, addrSpace );
    }

/*
    readPDE( (void *)PHYSMEM_START, &pde, (void *)getCR3() );
    writePDE( (void *)PHYSMEM_START, &pde, addrSpace );
*/

    if( ((int)threadAddr & KERNEL_VSTART) != KERNEL_VSTART )
    {
      thread->regs.cs = UCODE;
      thread->regs.ds = thread->regs.es = thread->regs.userSs = UDATA;
      thread->regs.ebp = (unsigned)uStack;
    }
    else if( tid == 0 )
    {
      thread->regs.cs = KCODE;
      thread->regs.ds = thread->regs.es = KDATA;
      thread->regs.esp = thread->regs.ebp = (unsigned)V_IDLE_STACK_TOP;
    }
    else
      assert(false);

    thread->regs.userEsp = (unsigned)uStack;

    thread->regs.eflags = 0x0201;//0x3201; // XXX: Warning: Magic Number
    thread->regs.eip = ( dword ) threadAddr;

    if( tid == 0 )
      memcpy( (void *)(/*PHYSMEM_START + */V_IDLE_STACK_TOP - sizeof(Registers)), (void *)&thread->regs, sizeof(Registers) );

    return thread;
}

/**
    Destroys a thread and detaches it from all queues.

    @param thread The TCB of the thread to release.
    @return 0 on success. -1 on failure.
*/

int releaseThread( TCB *thread )
{
  if( thread->state == DEAD )
    return -1;

  /* If the thread is a sender waiting for a recipient, remove the
     sender from the recipient's wait queue. If the thread is a
     receiver, clear its wait queue after
     waking all of the waiting threads. */

  switch( thread->state )
  {
    /* Assumes that a ready/running thread can't be on the timer queue. */

    case READY:
      for( int i=0; i < maxRunQueues; i++ )
      {
        if( isInQueue( &runQueues[i], GET_TID(thread) ) == true )
        {
          detachQueue( &runQueues[i], GET_TID(thread) );
          break;
        }
      }
      break;
    case WAIT_FOR_RECV:
    case WAIT_FOR_SEND:
    case SLEEPING:
      timerDetach( GET_TID(thread) );
      break;
  }

  if( thread->state == WAIT_FOR_SEND )
  {
    tid_t tid;

    while( (tid=popQueue( &thread->threadQueue )) != NULL_TID )
    {
      tcbTable[tid].quantaLeft = tcbTable[tid].priority + 1;
      tcbTable[tid].state = READY;
      attachRunQueue(&tcbTable[tid]);
    }
  }
  else if( thread->state == WAIT_FOR_RECV )
  {
    assert( thread->wait_tid != NULL_TID );

    detachQueue( &tcbTable[thread->wait_tid].threadQueue, GET_TID(thread) );
  }

  thread->state = DEAD;
  thread->quantaLeft = 0;
  thread->wait_tid = NULL_TID;
  thread->sig_handler = NULL;
  return 0;
}
