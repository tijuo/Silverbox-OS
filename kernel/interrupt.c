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
#include <kernel/bits.h>

static int handleHeapPageFault(int errorCode);

#define FIRST_VALID_PAGE        0x1000u

// The threads that are responsible for handling IRQs

tcb_t *irqHandlers[NUM_IRQS];
u32 pendingIrqBitmap[IRQ_BITMAP_COUNT];

/** Notify the interrupt controller that the IRQ has been handled and to re-enable the generation of IRQs.

   @param irqNum The IRQ number to re-enable.
*/

void endIRQ(unsigned int irqNum)
{
  enableIRQ(irqNum);
  sendEOI();
}

/** Set the IRQ handler for an IRQ.

   @param thread The thread that will handle the IRQ.
   @param irqnum The IRQ number.
*/

int registerIrq(tcb_t *thread, unsigned int irqNum)
{
  assert(thread != NULL);
  assert(isValidIRQ(irqNum));

  if(irqHandlers[irqNum])
    RET_MSG(irqHandlers[irqNum] == thread ? E_DONE : E_FAIL, "IRQ Handler has already been set");

  kprintf("TID %d registered IRQ: 0x%x\n", getTid(thread), irqNum);

  CLEAR_FLAG(pendingIrqBitmap[irqNum / sizeof(u32)], FROM_FLAG_BIT(irqNum & (sizeof(u32)-1)));
  irqHandlers[irqNum] = thread;
  enableIRQ(irqNum);
  return E_OK;
}

/** Reset the IRQ handler for an IRQ.

   @param irqnum The IRQ number.
*/

int unregisterIrq(unsigned int irqNum)
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

void handleIRQ(unsigned int irqNum, ExecutionState *state)
{
  assert(state != NULL);

  if(irqNum == TIMER_IRQ && !irqHandlers[TIMER_IRQ])
    timerInt(state);
  else if(irqNum == SPURIOUS_IRQ && !irqHandlers[SPURIOUS_IRQ])
    sendEOI(); // Stops the spurious IRQ7
  else
  {
    tcb_t *handler = irqHandlers[irqNum];

    sendEOI();
    disableIRQ(irqNum);

    if(handler && handler->threadState == IRQ_WAIT)
    {
      CLEAR_FLAG(pendingIrqBitmap[irqNum / sizeof(u32)], FROM_FLAG_BIT(irqNum & (sizeof(u32)-1)));
      setPriority(handler, HIGHEST_PRIORITY);
      handler->execState.eax = irqNum;
      startThread(handler);
    }
    else
      SET_FLAG(pendingIrqBitmap[irqNum / sizeof(u32)], FROM_FLAG_BIT(irqNum & (sizeof(u32)-1)));
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
  if(IS_FLAG_SET(errorCode, PAGING_ERR_PRES) || IS_FLAG_SET(errorCode, PAGING_ERR_USER))
    return E_PERM;

  addr_t faultAddr=(addr_t)getCR2();
  paddr_t pageFrame=NULL_PADDR;
  pde_t pde;
  pte_t pte;

  if(faultAddr < FIRST_VALID_PAGE)
    RET_MSG(E_PERM, "Error: NULL page access.");

  //kprintf("Page fault at 0x%x\n", faultAddr);

  if(faultAddr >= PAGETAB)
    RET_MSG(E_FAIL, "Page fault occurred in page table/directory region.");

  if(faultAddr >= KERNEL_HEAP_START && faultAddr <= KERNEL_HEAP_LIMIT)
  {
    if(IS_ERROR(readPmapEntry(NULL_PADDR, PDE_INDEX(faultAddr), &pde)))
      RET_MSG(E_FAIL, "Unable to read page directory entry.");
    else if(IS_ERROR(readPmapEntry((paddr_t)PFRAME_TO_ADDR(pde.base), PDE_INDEX(faultAddr), &pte)))
      RET_MSG(E_FAIL, "Unable to read page table entry.");
    else if(!pte.present)
    {
      pageFrame = allocPageFrame();

      if(pageFrame == NULL_PADDR)
        RET_MSG(E_FAIL, "Unable to allocate page frame.");
      else if(IS_ERROR(kMapPage(faultAddr, pageFrame, PAGING_RW | PAGING_SUPERVISOR)))
      {
        //kprintf("%x -> %x.\n", faultAddr, PFRAME_TO_ADDR(pageFrame));
        freePageFrame(pageFrame);
        RET_MSG(E_FAIL, "Unable to map page");
      }
    }
    else if(pte.usPriv || (!pte.rwPriv && IS_FLAG_SET(errorCode, PAGING_ERR_WRITE)))
    {
      if(pte.usPriv)
        kprintf("Page is marked as user.\n");

      if(!pte.rwPriv && IS_FLAG_SET(errorCode, PAGING_ERR_WRITE))
        kprintf("Page is marked as read-only (but a write access was performed).\n");

      RET_MSG(E_PERM, "Protection fault.");
    }
  }
  else
    RET_MSG(E_RANGE, "Fault address lies outside of heap range.");

  return E_OK;
}

/**
   Handles CPU exceptions. If the kernel is unable to handle an exception, it's
   sent to initial server to be handled.

   @param exNum The exception number.
   @param errorCode The error code that was pushed to the stack by the processor.
   @param state The saved execution state of the processor before the exception occurred.
*/


void handleCPUException(unsigned int exNum, int errorCode, ExecutionState *state)
{
  tcb_t *tcb = currentThread;

  assert(state);

  // Handle page faults due to non-present pages in kernel heap

  if(exNum == PAGE_FAULT_INT && !IS_ERROR(handleHeapPageFault(errorCode)))
      return;

  #if DEBUG

  if(tcb)
  {
    if( tcb == initServerThread )
    {
      if(exNum == PAGE_FAULT_INT)
      {
        addr_t faultAddress = (addr_t)getCR2();

        if(faultAddress >= INIT_SERVER_STACK_TOP - INIT_SERVER_STACK_SIZE && faultAddress < INIT_SERVER_STACK_TOP && (errorCode & PAGING_PRES) == 0
           && kMapPage(faultAddress, allocPageFrame(), PAGING_RW | PAGING_USER) == 0)
        {
          return;
        }
      }

      kprintf("Exception for initial server. System Halted.\n");
      dump_regs( tcb, state, exNum, errorCode );
      __asm__("hlt\n");
      return;
    }
//    else
//      dump_regs( tcb, state, exNum, errorCode );
  }
  #endif

  if( !tcb )
  {
    kprintf("NULL tcb. Unable to handle exception. System halted.\n");
    dump_state(state, exNum, errorCode);
    __asm__("hlt\n");
    return;
  }

  struct ExceptionMessage message = {
    .intNum       = exNum,
    .who          = getTid(tcb),
    .errorCode    = errorCode,
    .faultAddress = exNum == 14 ? getCR2() : 0
  };

  //kprintf("Sending exception %d message (err code: 0x%x, fault addr: 0x%x)\n", exNum, errorCode, exNum == 14 ? getCR2() : 0);

/*
  if(state->cs == 0x08)
  {
    kprintf("Exception in kernel:\n");
    dump_regs( tcb, state, exNum, errorCode );
  }
*/
  if(IS_ERROR(sendKernelMessage(INIT_SERVER_TID, EXCEPTION_MSG, &message, sizeof message)))
  {
    kprintf("Unable to send exception message to intial server\n");
    dump_regs( tcb, state, exNum, errorCode );

    releaseThread(tcb);
  }
}
