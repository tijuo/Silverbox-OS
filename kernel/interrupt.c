#include <kernel/thread.h>
#include <kernel/mm.h>
#include <kernel/debug.h>
#include <kernel/memory.h>
#include <kernel/schedule.h>
#include <kernel/lowlevel.h>
#include <kernel/interrupt.h>
#include <kernel/error.h>
#include <bits.h>
#include <stdint.h>

union idt_entry kernel_idt[NUM_EXCEPTIONS + NUM_IRQS];

// The threads that are responsible for handling IRQs

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


  RESTORE_STATE;
}

/**
 Handles CPU exceptions. If the kernel is unable to handle an exception, it's
 sent to initial server to be handled.

 @param interrupt_frame Pointer to the saved interrupt frame and processor execution state.
 */

void handle_cpu_exception(struct CpuExInterruptFrame *interrupt_frame) {
  tcb_t *tcb = get_current_thread();

  if(!tcb) {
	kprintf("NULL tcb. Unable to handle exception. System halted.\n");
	dump_state(&interrupt_frame->state, interrupt_frame->ex_num,
			   interrupt_frame->error_code);

	while(1) {
	  disable_int();
	  halt();
	}
  }

  RESTORE_STATE;
}
