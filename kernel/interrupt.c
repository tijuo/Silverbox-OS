#include <kernel/thread.h>
#include <kernel/mm.h>
#include <kernel/debug.h>
#include <kernel/memory.h>
#include <kernel/schedule.h>
#include <kernel/lowlevel.h>
#include <kernel/interrupt.h>
#include <kernel/error.h>
#include <os/msg/kernel.h>
#include <os/msg/init.h>
#include <kernel/bits.h>
#include <os/msg/message.h>
#include <stdalign.h>
#include <stdint.h>

#define HALT_OPCODE   0xF4u

#define FIRST_VALID_PAGE        0x1000u

union idt_entry kernel_idt[NUM_EXCEPTIONS + NUM_IRQS];

// The threads that are responsible for handling IRQs

tcb_t *irq_handlers[NUM_IRQS];
void handle_irq(struct IrqInterruptFrame *interrupt_frame);
void handle_cpu_exception(struct CpuExInterruptFrame *interrupt_frame);

#define CPU_HANDLER(num) \
    NAKED noreturn void cpu_ex##num##_handler(void); \
    NAKED noreturn void cpu_ex##num##_handler(void) { \
        SAVE_STATE; \
        __asm__("pushq $0\n" \
                "pushq $" #num "\n" \
                "mov %rsp, %rdi\n"  /* Push pointer to stack so that the stack will be aligned to 16-byte boundary upon end */ \
                "and $0xFFFFFFFFFFFFFFF0, %rsp\n" \
                "call handle_cpu_exception\n"); \
    }

#define CPU_ERR_HANDLER(num) \
    NAKED noreturn void cpu_ex##num##_handler(void); \
    NAKED noreturn void cpu_ex##num##_handler(void) { \
        SAVE_ERR_STATE; \
        __asm__("pushq $" #num "\n" \
                "mov %rsp, %rdi\n"  /* Push pointer to stack so that the stack will be aligned to 16-byte boundary upon end */ \
                "and $0xFFFFFFFFFFFFFFF0, %rsp\n" \
                "call handle_cpu_exception\n"); \
    }

#define IRQ_HANDLER(num) \
    NAKED noreturn void irq##num##_handler(void); \
    NAKED noreturn void irq##num##_handler(void) { \
        SAVE_STATE; \
        __asm__( \
                 "mov %rsp, %rdi\n"  /* Push pointer to stack so that the stack will be aligned to 16-byte boundary upon end */ \
                 "and $0xFFFFFFFFFFFFFFF0, %rsp\n" \
                 "call handle_irq\n"); \
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

void handle_irq(struct IrqInterruptFrame *frame) {
    tcb_t *current_thread = get_current_thread();
    tcb_t *new_thread = current_thread;

    unsigned int int_num = 0;

    //int irqNum = getInServiceIRQ();

    int_num -= IRQ_BASE;
    //tcb_t *handler = irqHandlers[intNum];

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
     if(currentThread->xsaveState)
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

     assert(newThread != currentThread);

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

void handle_cpu_exception(struct CpuExInterruptFrame *interrupt_frame) {
    tcb_t *tcb = get_current_thread();
//  ExecutionState *state = (ExecutionState*)(tss.esp0 + sizeof(uint32_t));
    unsigned long int msg_subject;

    if(!tcb) {
        kprintf("NULL tcb. Unable to handle exception. System halted.\n");
        dump_state(&interrupt_frame->state, interrupt_frame->ex_num,
                   interrupt_frame->error_code);

        while(1) {
            disable_int();
            halt();
        }
    }

    if(tcb->xsave_state)
        __asm__("fxsave %0\n" :: "m"(tcb->xsave_state));

    if(interrupt_frame->ex_num == 13
            && interrupt_frame->state.cs == UCODE_SEL
            && is_readable(interrupt_frame->state.rip, get_root_page_map())
            && *(uint8_t *)interrupt_frame->state.rip == HALT_OPCODE) {
        msg_subject = EXIT_MSG;
        struct ExitMessage message_data = {
            .status_code = interrupt_frame->state.rax,
            .who = get_tid(tcb)
        };

        __asm__(
            "movd (%0), %%xmm0\n"
            "movd 4(%0), %%xmm1\n"
            "xorpd %%xmm2, %%xmm2\n"
            "xorpd %%xmm3, %%xmm3\n"
            "xorpd %%xmm4, %%xmm4\n"
            "xorpd %%xmm5, %%xmm5\n"
            "xorpd %%xmm6, %%xmm6\n"
            "xorpd %%xmm7, %%xmm7\n"
            "xorpd %%xmm8, %%xmm8\n"
            "xorpd %%xmm9, %%xmm9\n"
            "xorpd %%xmm10, %%xmm10\n"
            "xorpd %%xmm11, %%xmm11\n"
            "xorpd %%xmm12, %%xmm12\n"
            "xorpd %%xmm13, %%xmm13\n"
            "xorpd %%xmm14, %%xmm14\n"
            "xorpd %%xmm15, %%xmm15\n"
            :: "r"(&message_data)
            : "xmm0", "xmm1", "xmm2", "xmm3",
            "xmm4", "xmm5", "xmm6", "xmm7",
            "xmm8", "xmm9", "xmm10", "xmm11",
            "xmm12", "xmm13", "xmm14", "xmm15");
    } else {
        msg_subject = EXCEPTION_MSG;

        alignas(16) struct ExceptionMessage message_data = {
            .rax = interrupt_frame->state.rax,
            .rbx = interrupt_frame->state.rbx,
            .rcx = interrupt_frame->state.rcx,
            .rdx = interrupt_frame->state.rdx,
            .rsi = interrupt_frame->state.rsi,
            .rdi = interrupt_frame->state.rdi,
            .rbp = interrupt_frame->state.rbp,
            .r8 = interrupt_frame->state.r8,
            .r9 = interrupt_frame->state.r9,
            .r10 = interrupt_frame->state.r10,
            .r11 = interrupt_frame->state.r11,
            .r12 = interrupt_frame->state.r12,
            .r13 = interrupt_frame->state.r13,
            .r14 = interrupt_frame->state.r14,
            .r15 = interrupt_frame->state.r15,
            .rsp =
            interrupt_frame->state.cs == KCODE_SEL ?
            tss.rsp0 + sizeof(uint64_t) + sizeof interrupt_frame->state :
            interrupt_frame->state.rsp,
            .cs = interrupt_frame->state.cs,
            .ds = interrupt_frame->state.ds,
            .es = interrupt_frame->state.es,
            .fs = interrupt_frame->state.fs,
            .gs = interrupt_frame->state.gs,
            .ss =
            interrupt_frame->state.cs == KCODE_SEL ? get_ss() :
            interrupt_frame->state.ss,
            .rflags = interrupt_frame->state.rflags,
            .cr0 = get_cr0(),
            .cr2 = get_cr2(),
            .cr3 = get_cr3(),
            .cr4 = get_cr4(),
            .cr8 = get_cr8(),
            .rip = tcb->user_exec_state.rip,
            .error_code = interrupt_frame->error_code,
            .fault_num = interrupt_frame->ex_num,
            .processor_id = get_current_processor(),
            .who = get_tid(tcb),
        };

        // FIXME: This is causing GPFs because messageData (and probably the stack)
        // FIXME: isn't aligned to 16 bytes

        __asm__(
            "movapd (%0),   %%xmm0\n"
            "movapd 16(%0), %%xmm1\n"
            "movapd 32(%0), %%xmm2\n"
            "movapd 48(%0), %%xmm3\n"
            "movapd 64(%0), %%xmm4\n"
            "movapd 80(%0), %%xmm5\n"
            "movapd 96(%0), %%xmm6\n"
            "movapd 112(%0), %%xmm7\n"
            "movapd 128(%0), %%xmm8\n"
            "movapd 144(%0), %%xmm9\n"
            "movapd 160(%0), %%xmm10\n"
            "movapd 176(%0), %%xmm11\n"
            "movapd 192(%0), %%xmm12\n"
            "movapd 208(%0), %%xmm13\n"
            "xorpd %%xmm14, %%xmm14\n"
            "xorpd %%xmm15, %%xmm15\n"
            :: "r"(&message_data)
            : "xmm0", "xmm1", "xmm2", "xmm3",
            "xmm4", "xmm5", "xmm6", "xmm7",
            "xmm8", "xmm9", "xmm10", "xmm11",
            "xmm12", "xmm13");
    }

    if(IS_ERROR(send_message(tcb, tcb->ex_handler, msg_subject, MSG_STD))) {
        kprintf("Unable to send message to exception handler.\n");

        if(msg_subject == EXCEPTION_MSG)
            kprintf("Tried to send exception %u message to tid %hhu.\n",
                    interrupt_frame->ex_num, interrupt_frame->error_code,
                    interrupt_frame->ex_num == 14 ? get_cr2() : 0, get_tid(tcb));
        else
            kprintf("Tried to send exit message (status code: %#x) to tid %hhu.\n",
                    interrupt_frame->state.rax, get_tid(tcb));
        dump_regs(tcb, &interrupt_frame->state, interrupt_frame->ex_num,
                  interrupt_frame->error_code);

        release_thread(tcb);
        switch_context(schedule(get_current_processor()));
    }

    RESTORE_STATE;
}
