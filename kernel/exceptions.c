#include <kernel/thread.h>
#include <kernel/mm.h>
#include <kernel/debug.h>
#include <oslib.h>
#include <kernel/memory.h>
#include <os/signal.h>

int sysEndIRQ( TCB *thread, int irqNum );
int sysRegisterInt( TCB *thread, int intNum );
int sysEndPageFault( TCB *thread, tid_t tid );
void handleIRQ(volatile TCB *thread  );
void handleCPUException(volatile TCB *thread );

extern int sysRaise( TCB *tcb, int signal, int arg );

static bool registerIRQ( int irq );
static bool releaseIRQ( int irq );
static void dump_regs( TCB *thread );

/* true = no handler set
   false = handler has been set */

bool IRQState[ 16 ] =
{
  true, true, false, true, true, true, true, true,
  true, false, true, true, true, true, true, true
};

tid_t IRQHandlers[ 16 ] = { NULL_TID, NULL_TID, NULL_TID, NULL_TID, NULL_TID, 
			       NULL_TID, NULL_TID, NULL_TID, NULL_TID, NULL_TID, 
			       NULL_TID, NULL_TID, NULL_TID, NULL_TID, NULL_TID, 
                               NULL_TID };

/* sysEndIRQ() should be called to complete the handling of
   an IRQ. */

int sysEndIRQ( TCB *thread, int irqNum )
{
  tid_t tid;
  assert( thread != NULL );
  kprintf("sysEndIRQ(): %d\n", irqNum);

  assert( irqNum >= IRQ0 && irqNum <= IRQ15 );

  if( thread == NULL )
    return -1;

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

/* This allows a thread to register an interrupt handler. All
   interrupts will be sent to a mailbox. */

int sysRegisterInt( TCB *thread, int intNum )
{
  assert( thread != NULL );
  assert( intNum >= IRQ0 && intNum <= IRQ15 );

  if( intNum - 0x20 >= 16 || intNum < 0x20 )
    return -1;

  if( registerIRQ( intNum ) == false )
    return -1;
  else
  {
    kprintf("Registering IRQ: 0x%x\n", intNum - 0x20);

    enableIRQ( intNum - IRQ0 );
    IRQHandlers[intNum - IRQ0] = GET_TID(thread);
    return 0;
  }
}

/* Notifies the kernel that a pager is finished handling a
   page fault, and it should resume execution. */

// XXX: This assumes that the page fault wasn't fatal

int sysEndPageFault( TCB *currThread, tid_t tid )
{
  assert( currThread != NULL );
  assert( tid != NULL_TID );
  assert( tcbTable[tid].exHandler != NULL_TID );

  if( currThread == NULL || tid == NULL_TID || tcbTable[tid].exHandler == NULL_TID )
    return -1;

  if( tcbTable[tid].exHandler != GET_TID(currThread) ) // Only a thread's exception handler should make this call
    return -1;

  // XXX: How do you tell the kernel that a fatal exception has occurred?

  startThread( &tcbTable[tid] );
  return 0;
}

/* If an IRQ occurs, the kernel will send a message to the
   thread's exception handler(if it exists). */

void handleIRQ(volatile TCB *thread )
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

    sysRaise( &tcbTable[IRQHandlers[regs->int_num - IRQ0]], ((byte)regs->int_num << 8) | SIGINT, 0 );

    return;
  }
}

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

static void dump_regs( TCB *thread )
{
  Registers *regs = (Registers *)&thread->regs;

  if( GET_TID(thread) == NULL_TID )
  {
    regs = (Registers *)V_IDLE_STACK_TOP;
    regs--;
  }

  kprintf( "\nException: %d @ EIP: 0x%x", regs->int_num, regs->eip );

  if( thread != NULL )
  {
    kprintf( " TID: %d", GET_TID(thread));
  }

  kprintf( "\nEAX: 0x%x EBX: 0x%x ECX: 0x%x EDX: 0x%x", regs->eax, regs->ebx, regs->ecx, regs->edx );
  kprintf( "\nESI: 0x%x EDI: 0x%x ESP: 0x%x EBP: 0x%x", regs->esi, regs->edi, regs->esp, regs->ebp );
  kprintf( "\nCS: 0x%x DS: 0x%x ES: 0x%x", regs->cs, regs->ds, regs->es );

  if( regs->int_num == 14 )
    kprintf(" CR2: 0x%x", getCR2());

  kprintf( " error code: 0x%x\n", regs->error_code );

  if( thread != NULL )
    kprintf( "CR3: 0x%x actual CR3: 0x%x", (unsigned)thread->addrSpace, getCR3() );

  kprintf("EFLAGS: 0x%x ", regs->eflags);

  if( thread != NULL )
  {
    if( (addr_t)thread->addrSpace != (addr_t)kernelAddrSpace )
      kprintf("User ESP: 0x%x User SS: 0x%x", regs->userEsp, regs->userSs);
  }
  kprintf("\n");
}

void handleCPUException(volatile TCB *thread  )
{
/*  TCB *thread = (TCB *)(*tssEsp0 - sizeof(Registers) - sizeof(dword));*/
//  struct Message *message = &__msg;
//  struct UMPO_Header *header = (struct UMPO_Header *)message->data;
//  struct ExceptionInfo *exInfo = (struct ExceptionInfo *)(header + 1);
  Registers *regs;

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

    if( tss->ioMap == 0x30000000 /*&& thread->io_bitmap != NULL*/ )
    {
      tss->ioMap = 0x68;
//      memcpy( (void *)TSS_IO_PERM_BMP, thread->io_bitmap, 8192 );
      kprintf("GPF due to IO bitmap\n");
      return;
    }
  }

  #if DEBUG

  if( GET_TID(thread) == IDLE_TID )
  {
    kprintf("Idle thread died. System halted.\n");
    dump_regs( (TCB *)thread );
    asm("hlt\n");
  }
  else if( GET_TID(thread) == 1 )
  {
    kprintf("Unable to handle exception. System Halted.\n");
    dump_regs( (TCB *)thread );
    asm("hlt\n");
  }
  else
  {
  //  kprintf("Exception. Sending notification...\n");
  //  dump_regs( (TCB *)thread );
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
