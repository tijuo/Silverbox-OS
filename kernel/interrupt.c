#include <kernel/thread.h>
#include <kernel/mm.h>
#include <kernel/debug.h>
#include <kernel/memory.h>
#include <kernel/schedule.h>
#include <kernel/lowlevel.h>
#include <kernel/paging.h>
#include <kernel/interrupt.h>
#include <kernel/error.h>
#include <os/msg/kernel.h>
#include <os/msg/init.h>
#include <kernel/bits.h>
#include <os/msg/message.h>
#include <stdint.h>

//static int handleHeapPageFault(int errorCode);

#define FIRST_VALID_PAGE        0x1000u

// The threads that are responsible for handling IRQs

tcb_t *irqHandlers[NUM_IRQS];

/**
   Processor exception handler for IRQs (from 0 to 31).

   If an IRQ occurs, the kernel will set a flag for which the
   corresponding IRQ handling thread can poll.

   @param irqNum The IRQ number.
   @param state The saved execution state of the processor before the exception occurred.
*/

void handleIRQ(unsigned int intNum, ExecutionState *state)
{
  tcb_t *currentThread = getCurrentThread();

  assert(state != NULL);

  //int irqNum = getInServiceIRQ();

  intNum -= IRQ_BASE;
  tcb_t *handler = irqHandlers[intNum];

      /*
#ifdef DEBUG
      if(irqNum == 0)
        incTimerCount();
#endif / * DEBUG * /

      disableIRQ((unsigned int)irqNum);
      sendEOI((unsigned int)irqNum);
*/
    if(handler)
    {
      __asm__("xsave %2\n" :: "a"(0x3), "d"(0), "m"(currentThread->xsaveState));
      __asm__("movd %0, %%mm0\n" :: "m"(intNum));

      if(IS_ERROR(sendMessage(currentThread, getTid(handler), IRQ_MSG, MSG_STD)))
      {
        kprintf("Unable to send irq %u message to handler.\n", intNum);
        __asm__("emms\n");
      }
    }

  __asm__("leave\n"
          "ret\n"
          "add $4, %esp\n");
  RESTORE_STATE;
  __asm__("iret\n");
}

/**
  Attempt to resolve a page fault occurring in the kernel's region of memory.

  @param errorCode The error code pushed to the stack by the processor.
  @return E_PERM if page fault is a protection fault (disallowed access, for example).
          E_FAIL if an error occurs that isn't supposed to happen. E_RANGE if page fault
          occurred outside of the kernel heap. E_OK on success.
*/

/*
 * The kernel doesn't have a heap.
 *
 *
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

  //kprintf("Page fault at %#x\n", faultAddr);

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
*/

/**
   Handles CPU exceptions. If the kernel is unable to handle an exception, it's
   sent to initial server to be handled.

   @param exNum The exception number.
   @param errorCode The error code that was pushed to the stack by the processor.
   @param state The saved execution state of the processor before the exception occurred.
*/


void handleCPUException(unsigned int exNum, int errorCode, ExecutionState *state)
{
  tcb_t *tcb = getCurrentThread();

  assert(state);

  /*
  // Handle page faults due to non-present pages in kernel heap

  if(exNum == PAGE_FAULT_INT && !IS_ERROR(handleHeapPageFault(errorCode)))
      return;
*/
  #ifdef DEBUG

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
          goto leave;
        }
      }

      kprintf("Exception for initial server. System Halted.\n");
      dump_regs( tcb, state, exNum, errorCode );

      while(1)
        __asm__("cli\n"
                "hlt\n");
    }
//    else
//      dump_regs( tcb, state, exNum, errorCode );
  }
  #endif

  if( !tcb )
  {
    kprintf("NULL tcb. Unable to handle exception. System halted.\n");
    dump_state(state, exNum, errorCode);

    while(1)
      __asm__("cli\n"
              "hlt\n");
  }

  struct ExceptionMessage messageData = {
      .intNum = exNum,
      .who = getTid(tcb),
      .errorCode = errorCode,
      .faultAddress = exNum == PAGE_FAULT_INT ? getCR2() : 0
  };

  uint64_t *ptr = (uint64_t *)&messageData;

  __asm__("xsave %2\n" :: "a"(0x3), "d"(0), "m"(tcb->xsaveState));
  __asm__("movq (%%eax), %%mm0\n"
          "movq 8(%%eax), %%mm1\n" :: "a"(ptr));

  //kprintf("Sending exception %u message (err code: %#x, fault addr: %#x)\n", exNum, errorCode, exNum == 14 ? getCR2() : 0);

  if(IS_ERROR(sendMessage(tcb, tcb->exHandler, EXCEPTION_MSG, MSG_STD)))
  {
    kprintf("Unable to send exception message to exception handler.\n");

    dump_regs( tcb, state, exNum, errorCode );
    __asm__("emms\n");

    releaseThread(tcb);
  }

leave:
  __asm__("leave\n"
          "ret\n"
          "add $8, %esp\n");
  RESTORE_STATE;
  __asm__("iret\n");
}
