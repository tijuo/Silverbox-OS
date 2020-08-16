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
  assert(thread != NULL);
  assert(isValidIRQ(irqNum));

  if(irqHandlers[irqNum])
    return E_FAIL;

  kprintf("TID %d registered IRQ: 0x%x\n", getTid(thread), irqNum);

  pendingIrqBitmap[irqNum / 32] &= ~(1 << (irqNum & 0x1F));
  irqHandlers[irqNum] = thread;
  enableIRQ(irqNum);
  return E_OK;
}

int unregisterIrq(int irqNum)
{
  assert(isValidIRQ(irqNum));

  irqHandlers[irqNum] = NULL;
  return E_OK;
}

/// Handles an IRQ (from 0 to 15).
/** If an IRQ occurs, the kernel will send a message to the
    thread that registered to handle the IRQ (if it exists). */

void handleIRQ(int irqNum, ExecutionState *state)
{
  assert(state != NULL);

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
      pendingIrqBitmap[irqNum / 32] |= (1 << (irqNum & 0x1F));
  }
}

int handleKernelPageFault(int errorCode)
{
  if(errorCode & (PAGING_ERR_PRES | PAGING_ERR_USER))
    return E_PERM;

  addr_t faultAddr=(addr_t)getCR2();
  paddr_t pageFrame=NULL_PADDR;
  int fail=0;
  pde_t pde;
  pte_t pte;

  if(faultAddr < 0x1000)
  {
    kprintf("Error: NULL page access.\n");
    return E_FAIL;
  }

  //kprintf("Page fault at 0x%x\n", faultAddr);

  if(faultAddr >= PAGETAB)
    return E_FAIL;

  if(!fail && faultAddr >= KERNEL_HEAP_START && faultAddr <= KERNEL_HEAP_LIMIT)
  {
    if(IS_ERROR(readPmapEntry(NULL_PADDR, PDE_INDEX(faultAddr), &pde))
    || IS_ERROR(readPmapEntry((paddr_t)(pde.base << 12), PDE_INDEX(faultAddr), &pte)))
    {
      fail = 1;
    }
    else if(!pte.present)
    {
      pageFrame = allocPageFrame();

      if(pageFrame == NULL_PADDR)
        fail = 1;
      else if(IS_ERROR(kMapPage(faultAddr, pageFrame, PAGING_RW | PAGING_SUPERVISOR)))
      {
        kprintf("Unable to map page: %x -> %x.\n", faultAddr, pageFrame << 12);
        freePageFrame(pageFrame);
        fail = 1;
      }
    }
    else if(pte.usPriv || (!pte.rwPriv && (errorCode & PAGING_ERR_WRITE)))
    {
      if(pte.usPriv)
        kprintf("Page is marked as user.\n");

      if(!pte.rwPriv && (errorCode & PAGING_ERR_WRITE))
        kprintf("Page is marked as read-only (but a write access was performed).\n");

      return E_PERM;
    }
  }
  else
  {
    kprintf("Fault address lies outside of heap range.\n");
    return E_RANGE;
  }

  if(fail)
    return E_FAIL;

  return E_OK;
}

/// Handles the CPU exceptions

void handleCPUException(int intNum, int errorCode, ExecutionState *state)
{
  tcb_t *tcb = currentThread;

  assert(state);

  // Handle page faults due to non-present pages in kernel heap

  if(intNum == 14 && !IS_ERROR(handleKernelPageFault(errorCode)))
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
  if(IS_ERROR(sendExceptionMessage(tcb, INIT_SERVER_TID, &message)))
  {
    kprintf("Unable to send exception message to intial server\n");
    dump_regs( tcb, state, intNum, errorCode );

    releaseThread(tcb);
  }
}
