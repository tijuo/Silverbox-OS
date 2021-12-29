#ifndef INTERRUPT_H
#define INTERRUPT_H

#include <kernel/thread.h>
#include <stdnoreturn.h>
#include <util.h>

#define PAGE_FAULT_INT    		14u

#define NUM_EXCEPTIONS    		32u
#define NUM_IRQS        		24u
#define is_valid_irq(irq)		({ __typeof__ (irq) _irq=(irq); (_irq < NUM_IRQS); })

struct CpuExInterruptFrame {
  unsigned long int ex_num;
  unsigned long int error_code;
  unsigned long int old_tss_esp0;
  exec_state_t state;
};

struct IrqInterruptFrame {
  unsigned long int old_tss_esp0;
  exec_state_t state;
};

NAKED noreturn void cpu_ex0_handler(void);
NAKED noreturn void cpu_ex1_handler(void);
NAKED noreturn void cpu_ex2_handler(void);
NAKED noreturn void cpu_ex3_handler(void);
NAKED noreturn void cpu_ex4_handler(void);
NAKED noreturn void cpu_ex5_handler(void);
NAKED noreturn void cpu_ex6_handler(void);
NAKED noreturn void cpu_ex7_handler(void);
NAKED noreturn void cpu_ex8_handler(void);
NAKED noreturn void cpu_ex9_handler(void);
NAKED noreturn void cpu_ex10_handler(void);
NAKED noreturn void cpu_ex11_handler(void);
NAKED noreturn void cpu_ex12_handler(void);
NAKED noreturn void cpu_ex13_handler(void);
NAKED noreturn void cpu_ex14_handler(void);
NAKED noreturn void cpu_ex15_handler(void);
NAKED noreturn void cpu_ex16_handler(void);
NAKED noreturn void cpu_ex17_handler(void);
NAKED noreturn void cpu_ex18_handler(void);
NAKED noreturn void cpu_ex19_handler(void);
NAKED noreturn void cpu_ex20_handler(void);
NAKED noreturn void cpu_ex21_handler(void);
NAKED noreturn void cpu_ex22_handler(void);
NAKED noreturn void cpu_ex23_handler(void);
NAKED noreturn void cpu_ex24_handler(void);
NAKED noreturn void cpu_ex25_handler(void);
NAKED noreturn void cpu_ex26_handler(void);
NAKED noreturn void cpu_ex27_handler(void);
NAKED noreturn void cpu_ex28_handler(void);
NAKED noreturn void cpu_ex29_handler(void);
NAKED noreturn void cpu_ex30_handler(void);
NAKED noreturn void cpu_ex31_handler(void);

NAKED noreturn void irq0_handler(void);
NAKED noreturn void irq1_handler(void);
NAKED noreturn void irq2_handler(void);
NAKED noreturn void irq3_handler(void);
NAKED noreturn void irq4_handler(void);
NAKED noreturn void irq5_handler(void);
NAKED noreturn void irq6_handler(void);
NAKED noreturn void irq7_handler(void);
NAKED noreturn void irq8_handler(void);
NAKED noreturn void irq9_handler(void);
NAKED noreturn void irq10_handler(void);
NAKED noreturn void irq11_handler(void);
NAKED noreturn void irq12_handler(void);
NAKED noreturn void irq13_handler(void);
NAKED noreturn void irq14_handler(void);
NAKED noreturn void irq15_handler(void);
NAKED noreturn void irq16_handler(void);
NAKED noreturn void irq17_handler(void);
NAKED noreturn void irq18_handler(void);
NAKED noreturn void irq19_handler(void);
NAKED noreturn void irq20_handler(void);
NAKED noreturn void irq21_handler(void);
NAKED noreturn void irq22_handler(void);
NAKED noreturn void irq23_handler(void);

/// The threads that are responsible for handling an IRQ

extern tcb_t *irq_handlers[NUM_IRQS];

#endif /* INTERRUPT_H */
