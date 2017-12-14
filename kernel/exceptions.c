#include <kernel/thread.h>
#include <kernel/mm.h>
#include <kernel/debug.h>
#include <kernel/memory.h>
#include <kernel/schedule.h>
#include <kernel/lowlevel.h>
#include <kernel/pic.h>
#include <kernel/paging.h>

#define NUM_IRQS	16

#define IRQ(x)    ((x)-IRQ0)
#define INT(x)    ((x)+IRQ0)

void endIRQ( int irqNum );
int registerInt( TCB *thread, int intNum );
void unregisterInt( int intNum );
void handleIRQ( TCB *thread, ExecutionState );
void handleCPUException( TCB *thread, ExecutionState );

void dump_regs( const TCB *thread );
void dump_state( const ExecutionState *state );
static void dump_stack( addr_t, addr_t );

/// Indicates whether an IRQ is allocated
/** true = no handler set
   false = handler has been set */

/// The threads that are responsible for handling an IRQ

static TCB *IRQHandlers[ NUM_IRQS ];

void endIRQ( int irqNum )
{
  enableIRQ(IRQ(irqNum);
  sendEOI();
}

/** This allows a thread to register an interrupt handler. All
   interrupts will be sent to the registered thread .
*/

int registerInt( TCB *thread, int intNum )
{
  if(!IRQHandlers[IRQ(intNum)] )
  {
    kprintf("Thread %d registered IRQ: 0x%x\n", GET_TID(thread), IRQ(intNum));

    IRQHandlers[IRQ(intNum)] = thread;
    return 0;
  }
  else
    return -1;
}

void unregisterInt( int intNum )
{
  IRQHandlers[IRQ(intNum)] = NULL;
}

/** Notifies the kernel that a pager is finished handling a
   page fault, and it should resume execution.

   Warning: This assumes that the page fault wasn't fatal.

   XXX: this doesn't need to be implemented in kernel mode.
*/

/// Handles an IRQ (from 0 to 15).
/** If an IRQ occurs, the kernel will send a message to the
    thread that registered to handle the IRQ (if it exists). */

void handleIRQ( TCB *thread, ExecutionState state )
{
  ExecutionState *execState;

  if( (unsigned int)(thread + 1) == tssEsp0 ) // User thread
    execState = (ExecutionState *)&thread->execState;
  else
    execState = (ExecutionState *)&state;

  if( execState->user.intNum == INT(7) && !IRQHandler[7] )
    sendEOI(); // Stops the spurious IRQ7
  else
  {
    disableIRQ(IRQ(execState->user.intNum));
    sendEOI();

    if( IRQHandlers[IRQ(execState->user.intNum)] == NULL )
      return;

    setPriority(IRQHandlers[IRQ(execState->user.intNum)], HIGHEST_PRIORITY);

    // XXX: Send a message to the handler

    return;
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
  else if( execState->user.intNum < IRQ0 )
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

    for( int i=2; i < 8; i++ )
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
}
