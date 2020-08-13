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
#include <os/msg/kernel.h>
#include <os/msg/init.h>

static int handleKernelPageFault(int errorCode);

// The threads that are responsible for handling IRQs

tcb_t *irqHandlers[NUM_IRQS];
int pendingIrqBitmap[IRQ_BITMAP_COUNT];

void endIRQ(int irqNum)
{
  enableIRQ(irqNum);
  sendEOI();
}

/** All interrupts will be sent to the registered port. */

int registerIrq(tcb_t *thread, int irqNum)
{
  if(!isValidIRQ(irqNum))
    return E_RANGE;
  if(thread == NULL)
    return E_INVALID_ARG;
  else if(!irqHandlers[irqNum])
  {
    kprintf("TID %d registered IRQ: 0x%x\n", getTid(thread), irqNum);

    pendingIrqBitmap[irqNum / 32] &= ~(1 << (irqNum & 0x1F));
    irqHandlers[irqNum] = thread;
    enableIRQ(irqNum);
    return E_OK;
  }
  else
    return E_FAIL;
}

int unregisterIrq(int irqNum)
{
  if(!isValidIRQ(irqNum))
    return E_RANGE;

  irqHandlers[irqNum] = NULL;
  return E_OK;
}

/// Handles an IRQ (from 0 to 15).
/** If an IRQ occurs, the kernel will send a message to the
    thread that registered to handle the IRQ (if it exists). */

void handleIRQ(int irqNum, ExecutionState *state)
{
  if(irqNum == 0 && !irqHandlers[0])
    timerInt(state);
  else if(irqNum == 7 && !irqHandlers[7])
    sendEOI(); // Stops the spurious IRQ7
  else
  {
    tcb_t *handler = irqHandlers[irqNum];

    sendEOI();
    disableIRQ(irqNum);

    if(handler && handler->threadState == IRQ_WAIT)
    {
      pendingIrqBitmap[irqNum / 32] &= ~(1 << (irqNum & 0x1F));
      setPriority(handler, HIGHEST_PRIORITY);
      handler->execState.eax = irqNum;
      startThread(handler);
    }
    else
    {
      pendingIrqBitmap[irqNum / 32] |= (1 << (irqNum & 0x1F));
      return;
    }
  }
}

int handleKernelPageFault(int errorCode)
{
  if(errorCode & (PAGING_ERR_PRES | PAGING_ERR_USER))
    return E_PERM;

  addr_t faultAddr = (addr_t)getCR2();
  paddr_t tableFrame=NULL_PADDR, pageFrame=NULL_PADDR;
  int fail = 0;
  pde_t mappedPde, *pde;
  u32 currentPDir = getCR3() & ~0x3FF;

  if(faultAddr < 0x1000)
  {
    kprintf("Error: NULL page access.\n");
    return E_FAIL;
  }

  //kprintf("Page fault at 0x%x\n", faultAddr);

  if(faultAddr >= PAGETAB)
    return E_FAIL;

  pde = ADDR_TO_PDE(faultAddr);

  if(currentPDir != initKrnlPDir
     && faultAddr >= KERNEL_TCB_START && faultAddr < PAGETAB)
  {
    if(!pde->present)
    {
      // Try to find the PDE in the bootstrap page directory

      if(readPmapEntry(initKrnlPDir, PDE_INDEX(faultAddr), &mappedPde) != E_OK)
        fail = 1;
      else
      {
        // If found, map it in this address space
        if(mappedPde.present)
        {
          if(writePmapEntry((paddr_t)currentPDir, PDE_INDEX(faultAddr), &mappedPde) != E_OK)
            fail = 1;
          else
            return E_OK;
        }
      }
    }
  }

  if(!fail && faultAddr >= KERNEL_HEAP_START && faultAddr <= KERNEL_HEAP_LIMIT)
  {
    // If the page table isn't mapped yet, map it.

    if(!pde->present)
    {
    /* create a new page table and map it in this address space and in
       the bootstrap page directory. */
      tableFrame = allocPageFrame();

      if(tableFrame == NULL_PADDR)
        fail = 1;
      else
      {
        pde_t newPde;

        newPde.base = (u32)(tableFrame >> 12);
        newPde.rwPriv = 1;
        newPde.usPriv = 0;
        newPde.present = 1;

        clearPhysPage(tableFrame);

        // Copy the new page table to the bootstrap page directory
        if(writePmapEntry(initKrnlPDir, PDE_INDEX(faultAddr), &newPde) != E_OK)
          fail = 1;
        else if(initKrnlPDir != currentPDir
                && kMapPageTable(faultAddr, tableFrame,
                                 PAGING_RW | PAGING_SUPERVISOR) != E_OK)
        {
          kprintf("Unable to map page table.\n");
          fail = 1;
        }
      }
    }

    pte_t *pte = ADDR_TO_PTE(faultAddr);

    if(!pte->present)
    {
      pageFrame = allocPageFrame();

      if(pageFrame == NULL_PADDR)
        fail = 1;
      else
      {
        if(kMapPage(faultAddr, pageFrame, PAGING_RW | PAGING_SUPERVISOR) != E_OK)
        {
          kprintf("Unable to map page.\n");
          freePageFrame(pageFrame);
          fail = 1;
        }
      }
    }

    if(!fail && (pte->usPriv || (!pte->rwPriv && (errorCode & PAGING_ERR_WRITE))))
    {
      if(pte->usPriv)
        kprintf("Page is marked as user.\n");

      if(!pte->rwPriv)
        kprintf("Page is marked as read-only (but a write access was performed).\n");

      return E_PERM;
    }

    if(fail)
    {
      if(pageFrame != NULL_PADDR)
        freePageFrame(pageFrame);

      return E_FAIL;
    }
  }
  else
  {
    kprintf("Fault address lies outside of heap range.\n");
    return E_RANGE;
  }

  return E_OK;
}

/// Handles the CPU exceptions

void handleCPUException(int intNum, int errorCode, ExecutionState *state)
{
  tcb_t *tcb = currentThread;

  // Handle page faults due to non-present pages in kernel heap

  if(intNum == 14 && handleKernelPageFault(errorCode) == E_OK)
    return;

  #if DEBUG

  if(tcb != NULL)
  {
    if( tcb == initServerThread )
    {
      kprintf("Exception for initial server. System Halted.\n");
      dump_regs( tcb, state, intNum, errorCode );
      __asm__("hlt\n");
      return;
    }
//    else
//      dump_regs( tcb, state, intNum, errorCode );
  }
  #endif

  if( tcb == NULL )
  {
    kprintf("NULL tcb. Unable to handle exception. System halted.\n");
    dump_state(state, intNum, errorCode);
    __asm__("hlt\n");
    return;
  }

  pem_t message = {
    .subject = EXCEPTION_MSG,
    .intNum = intNum,
    .who    = getTid(tcb),
    .errorCode = errorCode,
    .faultAddress = intNum == 14 ? getCR2() : 0 };

/*
  if(state->cs == 0x08)
  {
    kprintf("Exception in kernel:\n");
    dump_regs( tcb, state, intNum, errorCode );
  }
*/
  if(sendExceptionMessage(tcb, INIT_SERVER_TID, &message) != E_OK)
  {
    kprintf("Unable to send exception message to intial server\n");
    dump_regs( tcb, state, intNum, errorCode );

    releaseThread(tcb);
  }
}
