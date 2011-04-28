#include <kernel/thread.h>
#include <kernel/mm.h>
#include <kernel/debug.h>
#include <oslib.h>
#include <kernel/memory.h>
#include <kernel/schedule.h>
#include <os/signal.h>

int sysEndIRQ( TCB *thread, int irqNum );
int sysRegisterInt( TCB *thread, int intNum );
int sysUnregisterInt( TCB *thread, int intNum );
int sysEndPageFault( TCB *thread, tid_t tid );
void handleIRQ(volatile TCB *thread  );
void handleCPUException(volatile TCB *thread );

extern int sysRaise( TCB *tcb, int signal, int arg );

static bool registerIRQ( int irq );
static bool releaseIRQ( int irq );
void dump_regs( TCB *thread );

/// Indicates whether an IRQ is allocated
/** true = no handler set
   false = handler has been set */

bool IRQState[ 16 ] =
{
  true, true, false, true, true, true, true, true,
  true, false, true, true, true, true, true, true
};

/// The TIDs that are responsible for handling an IRQ

tid_t IRQHandlers[ 16 ] = { NULL_TID, NULL_TID, NULL_TID, NULL_TID, NULL_TID,
			       NULL_TID, NULL_TID, NULL_TID, NULL_TID, NULL_TID,
			       NULL_TID, NULL_TID, NULL_TID, NULL_TID, NULL_TID,
                               NULL_TID };

/* sysEndIRQ() should be called to complete the handling of
   an IRQ. */

int sysEndIRQ( TCB *thread, int irqNum )
{
  tid_t tid;
  kprintf("sysEndIRQ(): %d\n", irqNum);

  assert( irqNum >= IRQ0 && irqNum <= IRQ15 );

  if( irqNum >= IRQ0 && irqNum <= IRQ15 )
  {
    tid = IRQHandlers[ irqNum - IRQ0 ];

    if( tid != GET_TID(thread) )
      return -1;

    enableIRQ( irqNum - IRQ0 );
//    sendEOI();
    return 0;
  }
  else
    return -1;
}

/** This allows a thread to register an interrupt handler. All
   interrupts will be sent to the registered thread . */

int sysRegisterInt( TCB *thread, int intNum )
{
  if( intNum - 0x20 >= 16 || intNum < 0x20 )
    return -1;

  if( registerIRQ( intNum ) == true )
  {
    kprintf("Thread %d registered IRQ: 0x%x\n", GET_TID(thread), intNum - IRQ0);

    enableIRQ(intNum - IRQ0);
    IRQHandlers[intNum - IRQ0] = GET_TID(thread);
    return 0;
  }

  return -1;
}

int sysUnregisterInt( TCB *thread, int intNum )
{
  if( intNum - 0x20 >= 16 || intNum < 0x20 )
    return -1;
  else if( IRQHandlers[intNum - IRQ0] != GET_TID(thread) )
    return -1;
  else if( releaseIRQ( intNum ) == false )
    return -1;
  else
  {
    kprintf("IRQ 0x%x unregistered\n", intNum - IRQ0);

    disableIRQ(intNum - IRQ0);
    IRQHandlers[intNum - IRQ0] = NULL_TID;
  }

  return 0;
}

/** Notifies the kernel that a pager is finished handling a
   page fault, and it should resume execution.

   Warning: This assumes that the page fault wasn't fatal.
*/

int sysEndPageFault( TCB *currThread, tid_t tid )
{
  assert( tid != NULL_TID );
  assert( tcbTable[tid].exHandler != NULL_TID );

  if( tid == NULL_TID || tcbTable[tid].exHandler == NULL_TID )
    return -1;

  if( tcbTable[tid].exHandler != GET_TID(currThread) ) // Only a thread's exception handler should make this call
    return -1;

  // XXX: How do you tell the kernel that a fatal exception has occurred?

  if( currThread == &tcbTable[tid] )
  {
    kprintf("sysEndPageFault: currThread == &tcbTable[tid]\n");
  }

  startThread( &tcbTable[tid] );
  return 0;
}

/// Handles an IRQ (from 0 to 15).
/** If an IRQ occurs, the kernel will send a message to the
    thread that registered to handle the IRQ (if it exists). */

void handleIRQ( volatile TCB *thread )
{
  Registers *regs = (Registers *)&thread->regs;

  if( GET_TID(thread) == IDLE_TID )
  {
    regs = (Registers *)(V_IDLE_STACK_TOP);
    regs--;
  }

  if( regs->int_num == 0 )
  {
    dump_regs((TCB *)thread);
    return;
  }

  if( thread->regs.int_num == 39 && IRQState[39 - IRQ0] == true )
    sendEOI(); // Stops the spurious IRQ7
  else
  {
    disableIRQ( regs->int_num - IRQ0 );
    sendEOI();

    if( IRQHandlers[regs->int_num - IRQ0] == NULL_TID )
      return;

    setPriority(&tcbTable[IRQHandlers[regs->int_num - IRQ0]], HIGHEST_PRIORITY);

    sysRaise( &tcbTable[IRQHandlers[regs->int_num - IRQ0]], ((byte)regs->int_num << 8) | SIGINT, 0 );

    return;
  }
}

/// Allocates an IRQ (so it cannot be reused)

static bool registerIRQ( int irq )
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

static bool releaseIRQ( int irq )
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

/// Prints useful debugging information about the current thread

void dump_regs( TCB *thread )
{
  Registers *regs = (Registers *)&thread->regs;
  dword stack;

  if( GET_TID(thread) == NULL_TID )
  {
    regs = (Registers *)V_IDLE_STACK_TOP;
    regs--;
  }

  if( regs->int_num == 0x40 )
    kprintf("\nSyscall @ EIP: 0x%x", regs->eip);
  else
    kprintf( "\nException: 0x%x @ EIP: 0x%x", regs->int_num, regs->eip );

  kprintf( " TID: %d", GET_TID(thread));

  kprintf( "\nEAX: 0x%x EBX: 0x%x ECX: 0x%x EDX: 0x%x", regs->eax, regs->ebx, regs->ecx, regs->edx );
  kprintf( "\nESI: 0x%x EDI: 0x%x ESP: 0x%x EBP: 0x%x", regs->esi, regs->edi, regs->esp, regs->ebp );
  kprintf( "\nCS: 0x%x DS: 0x%x ES: 0x%x", regs->cs, regs->ds, regs->es );

  if( regs->int_num == 14 )
    kprintf(" CR2: 0x%x", getCR2());

  kprintf( " error code: 0x%x\n", regs->error_code );

  kprintf( "Address Space: 0x%x Actual CR3: 0x%x\n", (unsigned)thread->addrSpace, getCR3() );

  kprintf("EFLAGS: 0x%x ", regs->eflags);

  if( (addr_t)thread->addrSpace != (addr_t)kernelAddrSpace )
    kprintf("User ESP: 0x%x User SS: 0x%x", regs->userEsp, regs->userSs);

  kprintf("\n\nStack Trace:\n<Stack Frame>: Return-EIP args*\n");

  stack = regs->ebp;

  while( is_readable((void *)stack, thread->addrSpace) && is_readable((void *)(stack+4), thread->addrSpace))
  {
    if( *(dword *)stack < 0x100000 )
      break;

    kprintf("<0x%x>: 0x%x", *(dword *)stack, *(dword *)(stack + 4));

    for( int i=0; i < 6; i++ )
    {
      if( is_readable( (void *)(stack + 4 * (i+2)), thread->addrSpace) )
        kprintf(" 0x%x", *(dword *)(stack + 4 * (i+2)));
      else
        break;
    }

    kprintf("\n");

    if( !is_readable((void *)(*(dword *)stack), thread->addrSpace) )
      break;
    else
      stack = *(dword *)stack;
  }
}

/// Handles the CPU exceptions

void handleCPUException(volatile TCB *thread)
{
/*  TCB *thread = (TCB *)(*tssEsp0 - sizeof(Registers) - sizeof(dword));*/
//  struct ExceptionInfo *exInfo = (struct ExceptionInfo *)(header + 1);
  Registers *regs;

  if( thread == NULL )
  {
    kprintf("NULL thread. Unable to handle exception. System halted.\n");
    asm("hlt\n");
  }

  if( GET_TID(thread) == NULL_TID )
  {
    regs = (Registers *)V_IDLE_STACK_TOP;
    regs--;
  }
  else
    regs = (Registers *)&thread->regs;

  if( regs->int_num == 13 ) // check for an io operation
  {
    struct TSS_Struct *tss = (struct TSS_Struct *)KERNEL_TSS;
    pte_t pte;

    if( tss->ioMap == IOMAP_LAZY_OFFSET )
    {
      if( readPTE( (void *)TSS_IO_PERM_BMP, &pte, (void *)getCR3() ) == 0 &&
          pte.present &&
          pokeMem((void *)KERNEL_IO_BITMAP, 2 * PAGE_SIZE, (void *)TSS_IO_PERM_BMP, (void *)getCR3()) == 0 )
      {
        kprintf("Mapped IO Permission Bitmap\n");
        tss->ioMap = 0x68;
        return;
      }
    }
    kprintf("Unknown GPF\n");
    dump_regs( (TCB *)thread );
  }

  #if DEBUG

  if( GET_TID(thread) == IDLE_TID )
  {
    kprintf("Idle thread died. System halted.\n");
    dump_regs( (TCB *)thread );
    asm("hlt\n");
  }
  else if( GET_TID(thread) == init_server_tid )
  {
    kprintf("Exception for initial server. System Halted.\n");
    dump_regs( (TCB *)thread );
    asm("hlt\n");
  }
  #endif

  pauseThread( (TCB *)thread );

  if( thread->exHandler < 0 || thread->exHandler >= maxThreads || thread->exHandler == NULL_TID )
  {
    kprintf("Invalid exception handler\n");
    return;
  }

  if( sysRaise( &tcbTable[thread->exHandler], (GET_TID(thread) << 8) | SIGEXP, getCR2() ) < 0 )
  {
    kprintf("Exception handler not accepting exception\n");
    return;
  }
}
