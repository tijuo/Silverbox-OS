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

static int handleHeapPageFault(int errorCode);

// The threads that are responsible for handling IRQs

tcb_t *irqHandlers[NUM_IRQS];
int pendingIrqBitmap[IRQ_BITMAP_COUNT];

/** Notify the interrupt controller that the IRQ has been handled and to re-enable the generation of IRQs.

   @param irqNum The IRQ number to re-enable.
*/

void endIRQ(int irqNum)
{
  enableIRQ(irqNum);
  sendEOI();
}

/** Set the IRQ handler for an IRQ.

   @param thread The thread that will handle the IRQ.
   @param irqnum The IRQ number.
*/

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

/** Reset the IRQ handler for an IRQ.

   @param irqnum The IRQ number.
*/

int unregisterIrq(int irqNum)
{
  assert(isValidIRQ(irqNum));

  irqHandlers[irqNum] = NULL;
  return E_OK;
}

/**
   Processor exception handler for IRQs (from 0 to 15).

   If an IRQ occurs, the kernel will set a flag for which the
   corresponding IRQ handling thread can poll.

   @param irqNum The IRQ number.
   @param state The saved execution state of the processor before the exception occurred.
*/

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

/**
  Attempt to resolve a page fault occurring in the kernel's region of memory.

  @param errorCode The error code pushed to the stack by the processor.
  @return E_PERM if page fault is a protection fault (disallowed access, for example).
          E_FAIL if an error occurs that isn't supposed to happen. E_RANGE if page fault
          occurred outside of the kernel heap. E_OK on success.
*/

int handleHeapPageFault(int errorCode)
{
  if(errorCode & (PAGING_ERR_PRES | PAGING_ERR_USER))
    return E_PERM;

  addr_t faultAddr=(addr_t)getCR2();
  paddr_t pageFrame=NULL_PADDR;
  pde_t pde;
  pte_t pte;

  if(faultAddr < 0x1000)
  {
    kprintf("Error: NULL page access.\n");
    return E_PERM;
  }

  //kprintf("Page fault at 0x%x\n", faultAddr);

  if(faultAddr >= PAGETAB)
    return E_FAIL;

  if(faultAddr >= KERNEL_HEAP_START && faultAddr <= KERNEL_HEAP_LIMIT)
  {
    if(IS_ERROR(readPmapEntry(NULL_PADDR, PDE_INDEX(faultAddr), &pde))
    || IS_ERROR(readPmapEntry((paddr_t)(pde.base << 12), PDE_INDEX(faultAddr), &pte)))
    {
      return E_FAIL;
    }
    else if(!pte.present)
    {
      pageFrame = allocPageFrame();

      if(pageFrame == NULL_PADDR)
        return E_FAIL;
      else if(IS_ERROR(kMapPage(faultAddr, pageFrame, PAGING_RW | PAGING_SUPERVISOR)))
      {
        kprintf("Unable to map page: %x -> %x.\n", faultAddr, pageFrame << 12);
        freePageFrame(pageFrame);
        return E_FAIL;
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

  return E_OK;
}

/**
   Handles CPU exceptions. If the kernel is unable to handle an exception, it's
   sent to initial server to be handled.

   @param exNum The exception number.
   @param errorCode The error code that was pushed to the stack by the processor.
   @param state The saved execution state of the processor before the exception occurred.
*/


void handleCPUException(int exNum, int errorCode, ExecutionState *state)
{
  tcb_t *tcb = currentThread;

  assert(state);

  // Handle page faults due to non-present pages in kernel heap

  if(exNum == 14 && !IS_ERROR(handleHeapPageFault(errorCode)))
    return;

  #if DEBUG

  if(tcb != NULL)
  {
    if( tcb == initServerThread )
    {
      kprintf("Exception for initial server. System Halted.\n");
      dump_regs( tcb, state, exNum, errorCode );
      __asm__("hlt\n");
      return;
    }
//    else
//      dump_regs( tcb, state, exNum, errorCode );
  }
  #endif

  if( tcb == NULL )
  {
    kprintf("NULL tcb. Unable to handle exception. System halted.\n");
    dump_state(state, exNum, errorCode);
    __asm__("hlt\n");
    return;
  }

  pem_t message = {
    .subject = EXCEPTION_MSG,
    .intNum = exNum,
    .who    = getTid(tcb),
    .errorCode = errorCode,
    .faultAddress = exNum == 14 ? getCR2() : 0 };

/*
  if(state->cs == 0x08)
  {
    kprintf("Exception in kernel:\n");
    dump_regs( tcb, state, exNum, errorCode );
  }
*/
  if(IS_ERROR(sendExceptionMessage(tcb, INIT_SERVER_TID, &message)))
  {
    kprintf("Unable to send exception message to intial server\n");
    dump_regs( tcb, state, exNum, errorCode );

    releaseThread(tcb);
  }
}
