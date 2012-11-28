#include <kernel/thread.h>
#include <kernel/mm.h>
#include <kernel/debug.h>
#include <kernel/memory.h>
#include <kernel/schedule.h>
#include <os/signal.h>
#include <kernel/lowlevel.h>
#include <kernel/pic.h>
#include <kernel/paging.h>

#define NUM_IRQS	16

int sysEndIRQ( const TCB *thread, unsigned int irqNum );
int sysRegisterInt( TCB *thread, unsigned int intNum );
int sysUnregisterInt( TCB *thread, unsigned int intNum );
int sysEndPageFault( const TCB *thread, tid_t tid );
void handleIRQ( TCB *thread, ExecutionState );
void handleCPUException( TCB *thread, ExecutionState );

extern int sysRaise( TCB *tcb, int signal, int arg );

static bool registerIRQ( unsigned int irq );
static bool releaseIRQ( unsigned int irq );
void dump_regs( const TCB *thread );
void dump_state( const ExecutionState *state );
static void dump_stack( addr_t, addr_t );

/// Indicates whether an IRQ is allocated
/** true = no handler set
   false = handler has been set */

static bool IRQState[ NUM_IRQS ] =
{
  true, true, false, true, true, true, true, true,
  true, false, true, true, true, true, true, true
};

/// The threads that are responsible for handling an IRQ

static TCB *IRQHandlers[ NUM_IRQS ];

/* sysEndIRQ() should be called to complete the handling of
   an IRQ.

   XXX: This doesn't need to be implemented in kernel mode.
*/

int sysEndIRQ( const TCB *thread, unsigned int irqNum )
{
  kprintf("sysEndIRQ(): %d\n", irqNum);

  assert( irqNum >= IRQ0 && irqNum <= IRQ15 );

  if( irqNum >= IRQ0 && irqNum <= IRQ15 )
  {
    if( IRQHandlers[irqNum - IRQ0] != thread )
      return -1;

    enableIRQ( irqNum - IRQ0 );
//    sendEOI();
    return 0;
  }
  else
    return -1;
}

/** This allows a thread to register an interrupt handler. All
   interrupts will be sent to the registered thread .
*/

int sysRegisterInt( TCB *thread, unsigned int intNum )
{
  if( intNum < IRQ0 || intNum - IRQ0 >= NUM_IRQS )
    return -1;

  if( registerIRQ( intNum ) == true )
  {
    kprintf("Thread %d registered IRQ: 0x%x\n", GET_TID(thread), intNum - IRQ0);

    IRQHandlers[intNum - IRQ0] = thread;
    enableIRQ(intNum - IRQ0);
    return 0;
  }

  return -1;
}

int sysUnregisterInt( TCB *thread, unsigned int intNum )
{
  assert( intNum >= IRQ0 && intNum - IRQ0 < NUM_IRQS );

  if( intNum < IRQ0 || intNum - IRQ0 >= NUM_IRQS )
    return -1;
  else if( IRQHandlers[intNum - IRQ0] != thread )
    return -1;
  else if( releaseIRQ( intNum ) == false )
    return -1;
  else
  {
    kprintf("IRQ 0x%x unregistered\n", intNum - IRQ0);

    disableIRQ(intNum - IRQ0);
    IRQHandlers[intNum - IRQ0] = NULL;
  }

  return 0;
}

/** Notifies the kernel that a pager is finished handling a
   page fault, and it should resume execution.

   Warning: This assumes that the page fault wasn't fatal.

   XXX: this doesn't need to be implemented in kernel mode.
*/

int sysEndPageFault( const TCB *currThread, tid_t tid )
{
  assert( tid != NULL_TID );
  assert( tcbTable[tid].exHandler != NULL );

  if( tid == NULL_TID || tcbTable[tid].exHandler == NULL )
    return -1;

  if( tcbTable[tid].exHandler != currThread ) // Only a thread's exception handler should make this call
    return -1;

  // XXX: How do you tell the kernel that a fatal exception has occurred?

  if( currThread == &tcbTable[tid] )
  {
    kprintf("sysEndPageFault: currThread == &tcbTable[tid]\n");
  }

  if( startThread( &tcbTable[tid] ) != 0 )
    return -1;
  else
    return 0;
}

/// Handles an IRQ (from 0 to 15).
/** If an IRQ occurs, the kernel will send a message to the
    thread that registered to handle the IRQ (if it exists). */

void handleIRQ( TCB *thread, ExecutionState state )
{
  ExecutionState *execState;

  if( (unsigned int)(thread + 1) == tssEsp0 ) // User thread
  {
    execState = (ExecutionState *)&thread->execState;
  }
  else
  {
    execState = (ExecutionState *)&state;
  }

  if( execState->user.intNum == 39 && IRQState[39 - IRQ0] == true )
  {
    sendEOI(); // Stops the spurious IRQ7
  }
  else
  {
    disableIRQ( execState->user.intNum - IRQ0 );
    sendEOI();

    if( IRQHandlers[execState->user.intNum - IRQ0] == NULL )
      return;

    setPriority(IRQHandlers[execState->user.intNum - IRQ0], HIGHEST_PRIORITY);
    sysRaise( IRQHandlers[execState->user.intNum - IRQ0], ((byte)execState->user.intNum << 8) | SIGINT, 0 );

    return;
  }
}

/// Allocates an IRQ (so it cannot be reused)

static bool registerIRQ( unsigned int irq )
{
  assert( irq >= IRQ0 && irq <= IRQ15 );

  if ( irq < IRQ0 || irq > IRQ15 || IRQState[irq - IRQ0] == false )
    RET_MSG(false, "Invalid IRQ")//return false;
  else
  {
    IRQState[ irq - IRQ0 ] = false;
    return true;
  }
}

/// Deallocates an IRQ (so it can be reused)

static bool releaseIRQ( unsigned int irq )
{
  assert( irq >= IRQ0 && irq <= IRQ15 );

  if ( irq < IRQ0 || irq > IRQ15 )
    RET_MSG(false, "Invalid IRQ")//return false;
  else
  {
    IRQState[ irq - IRQ0 ] = true;
    return true;
  }
}

void dump_state( const ExecutionState *execState )
{
  if( execState == NULL )
  {
    kprintf("Unable to show execution state.\n");
    return;
  }

  if( execState->user.intNum == 0x40 )
  {
    kprintf("Syscall");
  }
  else if( execState->user.intNum < 32 )
  {
    kprintf("Exception %d", execState->user.intNum);
  }
  else if( execState->user.intNum >= IRQ0 && execState->user.intNum <= IRQ15 )
  {
    kprintf("IRQ%d", execState->user.intNum - IRQ0);
  }
  else
  {
    kprintf("Software Interrupt %d", execState->user.intNum);
  }

  kprintf(" @ EIP: 0x%x", execState->user.eip);

  kprintf( "\nEAX: 0x%x EBX: 0x%x ECX: 0x%x EDX: 0x%x", execState->user.eax, execState->user.ebx, execState->user.ecx, execState->user.edx );
  kprintf( "\nESI: 0x%x EDI: 0x%x ESP: 0x%x EBP: 0x%x", execState->user.esi, execState->user.edi, execState->user.esp, execState->user.ebp );
  kprintf( "\nCS: 0x%x DS: 0x%x ES: 0x%x", execState->user.cs, execState->user.ds, execState->user.es );

  if( execState->user.intNum == 14 )
  {
    kprintf(" CR2: 0x%x", getCR2());
  }

  kprintf( " error code: 0x%x\n", execState->user.errorCode );

  kprintf("EFLAGS: 0x%x ", execState->user.eflags);

  if( execState->user.cs == UCODE )
  {
    kprintf("User ESP: 0x%x User SS: 0x%x\n", execState->user.userEsp, execState->user.userSS);
  }
}

void dump_stack( addr_t stackFramePtr, addr_t addrSpace )
{
  kprintf("\n\nStack Trace:\n<Stack Frame>: [Return-EIP] args*\n");

  while( stackFramePtr )
  {
    kprintf("<0x%x>:", stackFramePtr);

    if( is_readable( stackFramePtr + 4, addrSpace ) )
    {
      kprintf(" [0x%x]", *(dword *)(stackFramePtr + 4));
    }
    else
    {
      kprintf(" [???]");
    }

    for( unsigned int i=2; i < 8; i++ )
    {
      if( is_readable( stackFramePtr + 4 * i, addrSpace) )
      {
        kprintf(" 0x%x", *(dword *)(stackFramePtr + 4 * i));
      }
      else
      {
        break;
      }
    }

    kprintf("\n");

    if( !is_readable(*(dword *)stackFramePtr, addrSpace) )
    {
      kprintf("<0x%x (invalid)>:\n", *(dword *)stackFramePtr);
      break;
    }
    else
    {
      stackFramePtr = *(dword *)stackFramePtr;
    }
  }
}

/// Prints useful debugging information about the current thread

void dump_regs( const TCB *thread )
{
  ExecutionState *execState=NULL;
  addr_t stackFramePtr;

  kprintf( "Thread: 0x%x ", thread, GET_TID(thread));

  if( ((addr_t)thread - (addr_t)tcbTable) % sizeof *thread == 0 &&
      GET_TID(thread) >= INITIAL_TID && GET_TID(thread) < MAX_THREADS )
  {
    kprintf("TID: %d ", GET_TID(thread));
  }
  else
  {
    kprintf("(invalid thread address) ");
  }

  if( (unsigned int)(thread + 1) == tssEsp0 ) // User thread
  {
    execState = (ExecutionState *)&thread->execState;
  }

  dump_state(execState);

  if( !execState )
  {
    kprintf("\n");
  }

  kprintf( "Thread CR3: 0x%x Current CR3: 0x%x\n", *(unsigned *)&thread->cr3, getCR3() );

  if( !execState )
  {
    __asm__("mov %%ebp, %0\n" : "=m"(stackFramePtr));

    if( !is_readable(*(dword *)stackFramePtr, thread->cr3.base << 12) )
    {
      kprintf("Unable to dump the stack\n");
      return;
    }

    stackFramePtr = *(addr_t *)stackFramePtr;
  }
  else
  {
    stackFramePtr = (addr_t)execState->user.ebp;
  }

  dump_stack(stackFramePtr, thread->cr3.base << 12);
}

/// Handles the CPU exceptions

void handleCPUException( TCB *thread, ExecutionState state)
{
  if( thread == NULL )
  {
    kprintf("NULL thread. Unable to handle exception. System halted.\n");
    dump_state(&state);
    __asm__("hlt\n");
    return;
  }

  #if DEBUG

  if( GET_TID(thread) == IDLE_TID )
  {
    kprintf("Idle thread died. System halted.\n");
    dump_regs( thread );
    dump_state(&state);
    __asm__("hlt\n");
    return;
  }
  else if( thread == init_server )
  {
    kprintf("Exception for initial server. System Halted.\n");
    dump_regs( thread );
    __asm__("hlt\n");
    return;
  }
  else
  {
    dump_regs( thread );
  }
  #endif

  pauseThread( thread );

  if( thread->exHandler == NULL || GET_TID(thread->exHandler) >= MAX_THREADS )
  {
    kprintf("Invalid exception handler\n");
    dump_regs( thread );
    return;
  }

  if( thread->exHandler == thread )
  {
    kprintf("Oops! The thread is its own exception handler!\n");
    dump_regs( thread );
    return;
  }

  if( sysRaise( thread->exHandler, (GET_TID(thread) << 8) | SIGEXP, getCR2() ) < 0 )
  {
    kprintf("Exception handler not accepting exception\n");
    dump_regs( thread );
    return;
  }
}
