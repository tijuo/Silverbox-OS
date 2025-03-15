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
#include <stdalign.h>
#include <stdint.h>

#define HALT_OPCODE 0xF4u

#define FIRST_VALID_PAGE 0x1000u

// The threads that are responsible for handling IRQs

tcb_t* irq_handlers[NUM_IRQS];
void handle_irq(struct IrqInterruptFrame* interrupt_frame);
void handle_cpu_exception(struct CpuExInterruptFrame* interrupt_frame);

#define CPU_HANDLER(num)                                                                                                      \
    NAKED noreturn void cpu_ex##num##_handler(void);                                                                          \
    NAKED noreturn void cpu_ex##num##_handler(void)                                                                           \
    {                                                                                                                         \
        SAVE_STATE;                                                                                                           \
        __asm__("push $0\n"                                                                                                   \
                "push $" #num "\n"                                                                                            \
                "mov %esp, %ecx\n" /* Push pointer to stack so that the stack will be aligned to 16-byte boundary upon end */ \
                "and $0xFFFFFFF0, %esp\n"                                                                                     \
                "mov %ecx, 4(%esp)\n"                                                                                         \
                "call handle_cpu_exception\n");                                                                               \
    }

#define CPU_ERR_HANDLER(num)                                                                                                  \
    NAKED noreturn void cpu_ex##num##_handler(void);                                                                          \
    NAKED noreturn void cpu_ex##num##_handler(void)                                                                           \
    {                                                                                                                         \
        SAVE_ERR_STATE;                                                                                                       \
        __asm__("push $" #num "\n"                                                                                            \
                "mov %esp, %ecx\n" /* Push pointer to stack so that the stack will be aligned to 16-byte boundary upon end */ \
                "and $0xFFFFFFF0, %esp\n"                                                                                     \
                "mov %ecx, 4(%esp)\n"                                                                                         \
                "call handle_cpu_exception\n");                                                                               \
    }

#define IRQ_HANDLER(num)                                                                                                  \
    NAKED noreturn void irq##num##_handler(void);                                                                         \
    NAKED noreturn void irq##num##_handler(void)                                                                          \
    {                                                                                                                     \
        SAVE_STATE;                                                                                                       \
        __asm__(                                                                                                          \
            "mov %esp, %ecx\n" /* Push pointer to stack so that the stack will be aligned to 16-byte boundary upon end */ \
            "and $0xFFFFFFF0, %esp\n"                                                                                     \
            "mov %ecx, 4(%esp)\n"                                                                                         \
            "call handle_irq\n");                                                                                         \
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

 @param interrupt_frame Pointer to the saved interrupt frame and processor execution state.

 */

    void handle_irq(struct IrqInterruptFrame* frame)
{
    tcb_t* current_thread = get_current_thread();
    tcb_t* new_thread = current_thread;

    unsigned int int_num = 0;

    // int irqNum = getInServiceIRQ();

    int_num -= IRQ_BASE;
    // tcb_t *handler = irqHandlers[intNum];

    /*
     #ifdef DEBUG
     if(irqNum == 0)
     incTimerCount();
     #endif / * DEBUG * /

     disableIRQ((unsigned int)irqNum);
     sendEOI((unsigned int)irqNum);
     */

     // Set an irq event instead
     /*
      if(handler) {
      if(currentThread != handler) {
      __asm__("fxsave %0\n" :: "m"(currentThread->xsaveState));
      __asm__("movd %0, %%xmm0\n"
      "xorpd %%xmm1, %%xmm1\n"
      "xorpd %%xmm2, %%xmm2\n"
      "xorpd %%xmm3, %%xmm3\n"
      "xorpd %%xmm4, %%xmm4\n"
      "xorpd %%xmm5, %%xmm5\n"
      "xorpd %%xmm6, %%xmm6\n"
      "xorpd %%xmm7, %%xmm7\n"
      :: "r"(intNum) : "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7");

      if(IS_ERROR(
      sendMessage(currentThread, getTid(handler), IRQ_MSG, MSG_STD))) {
      kprintf("Unable to send irq %u message to handler.\n", intNum);
      }
      }
      else if(currentThread->threadState != RUNNING) {
      wakeupThread(currentThread);
      }
      }

      if(currentThread->threadState != RUNNING) {
      newThread = schedule(getCurrentProcessor());

      kassert(newThread != currentThread);

      // todo: fxsave, do context switch, copy tss io bitmap...
      }
      */

    RESTORE_STATE;
}

/**
 Handles CPU exceptions. If the kernel is unable to handle an exception, it's
 sent to initial server to be handled.

 @param interrupt_frame Pointer to the saved interrupt frame and processor execution state.
 */

void handle_cpu_exception(struct CpuExInterruptFrame* interrupt_frame)
{
    tcb_t* tcb = get_current_thread();
    //  ExecutionState *state = (ExecutionState*)(tss.esp0 + sizeof(uint32_t));
    uint32_t msg_subject;

    if(!tcb) {
        kprintf("NULL tcb. Unable to handle exception. System halted.\n");
        dump_state(&interrupt_frame->state, interrupt_frame->ex_num,
            interrupt_frame->error_code);

        while(1) {
            disable_int();
            halt();
        }
    }

    __asm__("fxsave %0\n" ::"m"(tcb->xsave_state));

    if(interrupt_frame->ex_num == 13 && interrupt_frame->state.cs == UCODE_SEL && is_readable(interrupt_frame->state.eip, get_root_page_map()) && *(uint8_t*)interrupt_frame->state.eip == HALT_OPCODE) {
        msg_subject = EXIT_MSG;
        struct ExitMessage message_data = {
            .status_code = (int)interrupt_frame->state.eax,
            .who = get_tid(tcb) };

        __asm__(
            "movd (%0), %%xmm0\n"
            "movd 4(%0), %%xmm1\n"
            "xorpd %%xmm2, %%xmm2\n"
            "xorpd %%xmm3, %%xmm3\n"
            "xorpd %%xmm4, %%xmm4\n"
            "xorpd %%xmm5, %%xmm5\n"
            "xorpd %%xmm6, %%xmm6\n"
            "xorpd %%xmm7, %%xmm7\n" ::"r"(&message_data)
            : "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7");
    } else {
        msg_subject = EXCEPTION_MSG;

        alignas(16) struct ExceptionMessage message_data = {
            .eax = interrupt_frame->state.eax,
            .ebx = interrupt_frame->state.ebx,
            .ecx = interrupt_frame->state.ecx,
            .edx = interrupt_frame->state.edx,
            .esi = interrupt_frame->state.esi,
            .edi = interrupt_frame->state.edi,
            .ebp = interrupt_frame->state.ebp,
            .esp =
                interrupt_frame->state.cs == KCODE_SEL ? tss.esp0 + sizeof(uint32_t) + sizeof interrupt_frame->state - 2 * sizeof(uint32_t) : interrupt_frame->state.user_esp,
            .cs = interrupt_frame->state.cs,
            .ds = interrupt_frame->state.ds,
            .es = interrupt_frame->state.es,
            .fs = interrupt_frame->state.fs,
            .gs = interrupt_frame->state.gs,
            .ss =
                interrupt_frame->state.cs == KCODE_SEL ? get_ss() : interrupt_frame->state.user_ss,
            .eflags = interrupt_frame->state.eflags,
            .cr0 = get_cr0(),
            .cr2 = get_cr2(),
            .cr3 = get_cr3(),
            .cr4 = get_cr4(),
            .eip = tcb->user_exec_state.eip,
            .error_code = interrupt_frame->error_code,
            .fault_num = interrupt_frame->ex_num,
            .processor_id = get_current_processor(),
            .who = get_tid(tcb),
        };

        // FIXME: This is causing GPFs because messageData (and probably the stack)
        // FIXME: isn't aligned to 16 bytes

        __asm__(
            "movapd (%0), %%xmm0\n"
            "movapd 16(%0), %%xmm1\n"
            "movapd 32(%0), %%xmm2\n"
            "movapd 48(%0), %%xmm3\n"
            "movapd 64(%0), %%xmm4\n"
            "xorpd %%xmm5, %%xmm5\n"
            "xorpd %%xmm6, %%xmm6\n"
            "xorpd %%xmm7, %%xmm7\n" ::"r"(&message_data)
            : "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7");
    }

    if(IS_ERROR(send_message(tcb, tcb->ex_handler, msg_subject, MSG_STD))) {
        kprintf("Unable to send message to exception handler.\n");

        if(msg_subject == EXCEPTION_MSG)
            kprintf("Tried to send exception %u message to tid %hhu.\n",
                interrupt_frame->ex_num, interrupt_frame->error_code,
                interrupt_frame->ex_num == 14 ? get_cr2() : 0, get_tid(tcb));
        else
            kprintf("Tried to send exit message (status code: %#x) to tid %hhu.\n",
                interrupt_frame->state.eax, get_tid(tcb));
        dump_regs(tcb, &interrupt_frame->state, interrupt_frame->ex_num,
            interrupt_frame->error_code);

        release_thread(tcb);
    }

    __asm__("leave\n"
        "ret\n"
        "add $8, %esp\n");
    RESTORE_STATE;
}
