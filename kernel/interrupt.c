#include <kernel/thread.h>
#include <kernel/mm.h>
#include <kernel/debug.h>
#include <kernel/memory.h>
#include <kernel/schedule.h>
#include <kernel/lowlevel.h>
#include <kernel/pic.h>
#include <kernel/paging.h>
#include <kernel/interrupt.h>

#define NUM_IRQS	16

#define IRQ(x)    ((x)-IRQ0)
#define INT(x)    ((x)+IRQ0)

void handleIRQ( TCB *thread, ExecutionState );
void handleCPUException( TCB *thread, ExecutionState );

/// The threads that are responsible for handling an IRQ

static TCB *IRQHandlers[ NUM_IRQS ];

void endIRQ( int irqNum )
{
  enableIRQ(IRQ(irqNum));
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

  if( execState->user.intNum == INT(7) && !IRQHandlers[7] )
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
