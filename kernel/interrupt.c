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
void handleIRQ(void);
void handleCpuException(uint32_t intNum, uint32_t errorCode);

#define CPU_HANDLER(num) \
NAKED noreturn void cpuEx##num##Handler(void); \
NAKED noreturn void cpuEx##num##Handler(void) { \
  SAVE_STATE; \
  __asm__("push $0\n" \
          "push $" #num "\n" \
          "call handleCpuException\n"); \
}

#define CPU_ERR_HANDLER(num) \
NAKED noreturn void cpuEx##num##Handler(void); \
NAKED noreturn void cpuEx##num##Handler(void) { \
  SAVE_ERR_STATE; \
  __asm__("push $" #num "\n" \
          "call handleCpuException\n"); \
}

#define IRQ_HANDLER(num) \
NAKED noreturn void irq##num##Handler(void); \
NAKED noreturn void irq##num##Handler(void) { \
  SAVE_STATE; \
  __asm__("call handleIRQ\n"); \
}

CPU_HANDLER(0)
CPU_HANDLER(1)
CPU_HANDLER(2)
CPU_HANDLER(3)
CPU_HANDLER(4)
CPU_HANDLER(5)
CPU_HANDLER(6)
CPU_HANDLER(7)
CPU_ERR_HANDLER(8)
CPU_HANDLER(9)
CPU_ERR_HANDLER(10)
CPU_ERR_HANDLER(11)
CPU_ERR_HANDLER(12)
CPU_ERR_HANDLER(13)
CPU_ERR_HANDLER(14)
CPU_HANDLER(15)
CPU_HANDLER(16)
CPU_ERR_HANDLER(17)
CPU_HANDLER(18)
CPU_HANDLER(19)
CPU_HANDLER(20)
CPU_ERR_HANDLER(21)
CPU_HANDLER(22)
CPU_HANDLER(23)
CPU_HANDLER(24)
CPU_HANDLER(25)
CPU_HANDLER(26)
CPU_HANDLER(27)
CPU_HANDLER(28)
CPU_HANDLER(29)
CPU_HANDLER(30)
CPU_HANDLER(31)

IRQ_HANDLER(0)
IRQ_HANDLER(1)
IRQ_HANDLER(2)
IRQ_HANDLER(3)
IRQ_HANDLER(4)
IRQ_HANDLER(5)
IRQ_HANDLER(6)
IRQ_HANDLER(7)
IRQ_HANDLER(8)
IRQ_HANDLER(9)
IRQ_HANDLER(10)
IRQ_HANDLER(11)
IRQ_HANDLER(12)
IRQ_HANDLER(13)
IRQ_HANDLER(14)
IRQ_HANDLER(15)
IRQ_HANDLER(16)
IRQ_HANDLER(17)
IRQ_HANDLER(18)
IRQ_HANDLER(19)
IRQ_HANDLER(20)
IRQ_HANDLER(21)
IRQ_HANDLER(22)
IRQ_HANDLER(23)

/**
 Interrupt handler for IRQs.

 If an IRQ occurs, the kernel will set a flag for which the
 corresponding IRQ handling thread can poll.

 @param irqNum The IRQ number.
 @param state The saved execution state of the processor before the exception occurred.
 */

void handleIRQ(void) {
  tcb_t *currentThread = getCurrentThread();
  tcb_t *newThread = currentThread;

  unsigned int intNum = 0;

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
  if(handler) {
    if(currentThread != handler) {
      __asm__("fxsave %0\n" :: "m"(currentThread->xsaveState));
      __asm__("movd %0, %%mm0\n" :: "r"(intNum));

      if(IS_ERROR(
          sendMessage(currentThread, getTid(handler), IRQ_MSG, MSG_STD))) {
        kprintf("Unable to send irq %u message to handler.\n", intNum);
        __asm__("emms\n");
      }
    }
    else if(currentThread->threadState != RUNNING) {
      wakeupThread(currentThread);
    }
  }

  if(currentThread->threadState != RUNNING) {
    newThread = schedule(getCurrentProcessor());

    assert(newThread != currentThread);

    // todo: fxsave, do context switch, copy tss io bitmap...
  }

  RESTORE_STATE;
}

/**
 Handles CPU exceptions. If the kernel is unable to handle an exception, it's
 sent to initial server to be handled.

 @param exNum The exception number.
 @param errorCode The error code that was pushed to the stack by the processor.
 @param state The saved execution state of the processor before the exception occurred.
 */

void handleCpuException(uint32_t exNum, uint32_t errorCode) {
  tcb_t *tcb = getCurrentThread();

  if(!tcb) {
    kprintf("NULL tcb. Unable to handle exception. System halted.\n");
    dump_state((ExecutionState*)(&errorCode + 1), exNum, errorCode);

    while(1) {
      disableInt();
      halt();
    }
  }

  struct ExceptionMessage messageData = {
    .who = getTid(tcb),
    .errorCode = errorCode,
    .faultAddress = exNum == PAGE_FAULT_INT ? getCR2() : 0,
    .intNum = exNum
  };

  __asm__("fxsave %0\n" :: "m"(tcb->xsaveState));
  __asm__("movq (%0), %%mm0\n"
          "movq 8(%0), %%mm1\n" :: "r"(&messageData));

  if(IS_ERROR(sendMessage(tcb, tcb->exHandler, EXCEPTION_MSG, MSG_STD))) {
    kprintf("Unable to send exception message to exception handler.\n");
    kprintf(
        "Tried to send exception %u message (err code: %#x, fault addr: %#x) to tid %hhu.\n",
        exNum, errorCode, exNum == 14 ? getCR2() : 0, getTid(tcb));

    dump_regs(tcb, &tcb->userExecState, exNum, errorCode);
    __asm__("emms\n");

    releaseThread(tcb);
  }

  __asm__("leave\n"
      "ret\n");
  RESTORE_STATE;
}
