#include <kernel/debug.h>
#include <oslib.h>
#include <string.h>
#include <kernel/thread.h>
#include <kernel/schedule.h>
#include <kernel/mm.h>

#define DEFAULT_USTACK 0xCAFED00D

//extern TCB *idleThread;

//extern void saveAndSwitchContext( volatile TCB * volatile, volatile TCB * volatile );

/// Returns an unused TID

tid_t getFreeTID(void)
{
  static tid_t lastTID = INITIAL_TID;
  int count = 0;

  kprintf("Last tid: %d\n", lastTID);

  if( tcbTable[lastTID].state == DEAD )
    kprintf("Last state is still DEAD(ok the first time).\n");

  while( tcbTable[lastTID].state != DEAD && count++ < maxThreads )
    lastTID = (lastTID == maxThreads - 1 ? INITIAL_TID + 1 : lastTID + 1);

  kprintf("allocated tid\n");

  if( tcbTable[lastTID].state == DEAD )
    return lastTID;
  else
    RET_MSG(NULL_TID, "Exhausted all TIDS!");
}

/*
int switchToThread( volatile TCB *oldThread, volatile TCB *newThread )
{
  void *stack;
  TCB *_oldThread, *_newThread;

  assert( newThread != NULL );
  assert( oldThread != NULL );

  if( newThread == NULL || oldThread == NULL )
    return -1;

  _oldThread = (TCB *)oldThread;
  _newThread = (TCB *)newThread;

  // disableInt();

  stack = _oldThread->stack;

//  if( newThread->process->tssIOBitmap != NULL )
//      memcpy( tssIOBitmap, newThread->process->tssIOBitmap, 128 );

  saveAndSwitchContext( _oldThread, _newThread ); // <-- Low level

  _oldThread->stack = stack;

  // enableInt();

  return 0;
}
*/

/// Starts a non-running thread

int startThread( TCB *thread )
{
  assert( thread != NULL );

  if( thread == NULL )
    return -1;

  if( thread->state == PAUSED ) /* A paused queue really isn't necessary. */
  {
    assert( thread != currentThread ); // A paused thread should NEVER be running
    //detachPausedQueue( thread );
    thread->state = READY;
    thread->quantaLeft = thread->priority + 1;

    assert(thread->state != DEAD);

    attachRunQueue( thread );
    return 0;
  }
  else
    return 1;
}


/// Temporarily pauses a thread

int sleepThread( TCB *thread, int msecs )
{
  assert( thread != NULL );
  assert( msecs > 0 );

  if( thread->state == SLEEPING )
    RET_MSG(-1, "Already sleeping!")//return -1;

  if( thread->state != READY && thread->state != RUNNING )
  {
    kprintf("sleepThread() failed!\n");
    assert(false);
    return -1;
  }

  if( thread->state == READY && GET_TID(thread) != IDLE_TID )
    detachRunQueue( thread );

  if( sleepEnqueue( &sleepQueue, GET_TID(thread), (msecs * HZ) / 1000 ) != 0 )
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
  assert( thread != NULL );

  if( thread == NULL )
    RET_MSG(-1, "NULL ptr")

  switch( thread->state )
  {
    case READY:
      detachRunQueue( thread );
    case RUNNING:
      //attachPausedQueue( thread ); /* A paused queue isn't necessary */
      thread->state = PAUSED;
      return 0;
    case PAUSED:
      RET_MSG(1, "Already Paused")//return 1;
    default:
      RET_MSG(-1, "Paused from other state")//return -1;
  }
}

/* Note that 'stack' is a physical address that will be mapped below 0xFF800000 */

/// Creates and initializes a new thread

TCB *createThread( addr_t threadAddr, addr_t addrSpace, addr_t uStack, tid_t exHandler )
{
   TCB * thread;
   tid_t tid = NULL_TID;
   pde_t pde;
// This should be changed
//   struct RegisterState *state;

   assert(addrSpace != (addr_t)NULL_PADDR);
   assert( threadAddr != NULL );
   assert( exHandler != NULL_TID );

   if( threadAddr == NULL )
     RET_MSG(NULL, "NULL thread addr")
   else if( exHandler == NULL_TID )
     RET_MSG(NULL, "NULL exHandler")
   else if( addrSpace == NULL_PADDR )
     RET_MSG(NULL, "NULL addrSpace")

   // disableInt();

   tid = getFreeTID();

   if( tid == NULL_TID )
     RET_MSG(NULL, "NULL tid")

    thread = &tcbTable[tid];

   kprintf("TID: %d Thread: 0x%x\n", tid, thread);

    thread->priority = NORMAL_PRIORITY;
    thread->quantaLeft = 0;
    thread->state = PAUSED;
    thread->addrSpace = addrSpace;
    thread->exHandler = exHandler;
    thread->wait_tid = NULL_TID;
    thread->threadQueue.tail = thread->threadQueue.head = NULL_TID;
    thread->sig_handler = NULL;
//    thread->io_bitmap = NULL;

//    thread->stack = thread->stackMem + THREAD_STACK_LEN - sizeof( struct RegisterState );
//    state = (struct RegisterState *)thread->regs;

    assert( thread->addrSpace == tcbTable[tid].addrSpace );
    assert( (u32)addrSpace == ((u32)addrSpace & ~0xFFF) );

    *(u32 *)&pde = (u32)addrSpace | PAGING_RW | PAGING_PRES;
    writePDE( (void *)PAGETAB, &pde, addrSpace );

    readPDE( (void *)KERNEL_VSTART, &pde, (void *)getCR3() );
    writePDE( (void *)KERNEL_VSTART, &pde, addrSpace );
    readPDE( (void *)PHYSMEM_START, &pde, (void *)getCR3() );
    writePDE( (void *)PHYSMEM_START, &pde, addrSpace );

//    thread->tid = tid;

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
      // this might not work
//      thread->regs.esp = thread->regs.ebp =(unsigned)(V_IDLE_STACK_TOP - sizeof(Registers));
    }
    else
      assert(false);

    thread->regs.userEsp = (unsigned)uStack;

    thread->regs.eflags = 0x3201;//0x3201; // XXX: Warning: Magic Number
    thread->regs.eip = ( dword ) threadAddr;

    if( tid == 0 )
      memcpy( (void *)(/*PHYSMEM_START + */V_IDLE_STACK_TOP - sizeof(Registers)), (void *)&thread->regs, sizeof(Registers) );

//    attachPausedQueue( thread );
    // enableInt();
    return thread;
}

/// Deallocates a thread

int releaseThread( TCB *currThread, tid_t tid )
{
  assert( currThread != NULL );
  assert( tid != NULL_TID );

  if( currThread == NULL || tid == NULL_TID )
    return -1;

  // disableInt();

  tcbTable[tid].state = DEAD;

  // enableInt();

  return 0;
}
