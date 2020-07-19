#include <kernel/thread.h>
#include <kernel/mm.h>
#include <kernel/debug.h>
#include <kernel/memory.h>
#include <kernel/schedule.h>
#include <kernel/lowlevel.h>
#include <kernel/pic.h>
#include <kernel/paging.h>
#include <kernel/interrupt.h>
#include <kernel/error.h>

static bool handleHeapPageFault(void);

pid_t IRQHandlers[NUM_IRQS];

// The threads that are responsible for handling IRQs
TCB *irqThreads[NUM_IRQS];
pid_t irqPorts[NUM_IRQS];

void endIRQ(int irqNum)
{
  enableIRQ(irqNum);
  sendEOI();
}

/** All interrupts will be sent to the registered port. */

int registerInt(pid_t port, int intNum)
{
  if(intNum < 0 || intNum > NUM_IRQS-1 || port == NULL_PID)
    return -1;
  else if(IRQHandlers[intNum] == NULL_PID)
  {
    kprintf("Port %d registered IRQ: 0x%x\n", port, intNum);

    IRQHandlers[intNum] = port;
    enableIRQ(intNum);
    return 0;
  }
  else
    return -1;
}

void unregisterInt(int intNum)
{
  if(intNum < 0 || intNum > NUM_IRQS-1)
    return;

  IRQHandlers[intNum] = NULL_PID;
}

/// Handles an IRQ (from 0 to 15).
/** If an IRQ occurs, the kernel will send a message to the
    thread that registered to handle the IRQ (if it exists). */

void handleIRQ(int irqNum, TCB *thread, ExecutionState state)
{
  if(irqNum == 0)
    timerInt(thread);
  else if(irqNum == 7 && IRQHandlers[7] == NULL_PID)
    sendEOI(); // Stops the spurious IRQ7
  else
  {
    sendEOI();
    disableIRQ(irqNum);

    if(IRQHandlers[irqNum] == NULL_PID)
      return;

    setPriority(getTcb(getPort(IRQHandlers[irqNum])->owner), HIGHEST_PRIORITY);

    struct PortPair pair;

    pair.local = NULL_PID;
    pair.remote = IRQHandlers[irqNum];

    int args[5] = { IRQ_MSG, irqNum, 0, 0, 0 };

    if(sendMessage(irqThreads[irqNum], pair, 1, args) != 0)
      kprintf("Failed to send IRQ%d message to port %d.\n", irqNum, pair.local);
  }
}

bool handleHeapPageFault(void)
{
  addr_t faultAddr = (addr_t)getCR2();

  if(faultAddr >= KERNEL_HEAP_START && faultAddr <= KERNEL_HEAP_LIMIT)
  {
    pde_t *pde = ADDR_TO_PDE(faultAddr);

    // If the page table isn't mapped yet, map it.

    if(!pde->present)
    {
      if(kMapPageTable(faultAddr, allocPageFrame(), PAGING_RW | PAGING_SUPERVISOR) != E_OK)
        return false;
    }

    pte_t *pte = ADDR_TO_PTE(faultAddr);

    if(!pte->present)
    {
      if(kMapPage(faultAddr, allocPageFrame(), PAGING_RW | PAGING_SUPERVISOR) != E_OK)
        return false;
    }

    if(pte->usPriv || !pte->rwPriv)
      return false;
  }

  return true;
}

/// Handles the CPU exceptions

void handleCPUException(int intNum, int errorCode, TCB *tcb,
                        ExecutionState state)
{
  #if DEBUG

  if(tcb != NULL)
  {
    if( tcb->tid == IDLE_TID )
    {
      kprintf("Exception for idle thread. System halted.\n");
      dump_regs( tcb, &state, intNum, errorCode );
      dump_state(&state, intNum, errorCode);
      __asm__("hlt\n");
      return;
    }
    else if( tcb == init_server )
    {
      kprintf("Exception for initial server. System Halted.\n");
      dump_regs( tcb, &state, intNum, errorCode );
      __asm__("hlt\n");
      return;
    }
    else
    {
      dump_regs( tcb, &state, intNum, errorCode );
    }
  }
  #endif

  // Handle page faults due to non-present pages in kernel heap

  if( intNum == 14
      && (errorCode & 0xF) == PAGING_ERR_SUPERVISOR
      && handleHeapPageFault() )
  {
    return;
  }

  if( tcb == NULL )
  {
    kprintf("NULL tcb. Unable to handle exception. System halted.\n");
    dump_state(&state, intNum, errorCode);
    __asm__("hlt\n");
    return;
  }


  if( tcb->exHandler == NULL_PID )
  {
    kprintf("TID: %d has no exception handler. Releasing...\n", tcb->tid);
    dump_regs( tcb, &state, intNum, errorCode );
    releaseThread(tcb);
  }
  else if( getPort(tcb->exHandler)->owner == tcb->tid )
  {
    kprintf("Oops! TID %d is its own exception handler! Releasing...\n", tcb->tid);
    dump_regs( tcb, &state, intNum, errorCode );
    releaseThread(tcb);
  }
  else
  {
    struct PortPair pair;
    pair.local = NULL_PID;
    pair.remote = tcb->exHandler;
    int args[5] = { EXCEPTION_MSG, tcb->tid, intNum,
                    errorCode, intNum == 14 ? getCR2() : 0 };

    if(sendMessage(tcb, pair, 1, args) != 0)
    {
      kprintf("Unable to send exception message to PID: %d\n", tcb->exHandler);
      releaseThread(tcb);
    }
    else
      pauseThread(tcb);
  }
}
