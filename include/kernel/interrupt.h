#ifndef INTERRUPT_H
#define INTERRUPT_H

#include <kernel/thread.h>
#include <kernel/message.h>
#include <stdnoreturn.h>
#include <util.h>

#define PAGE_FAULT_INT    		14u

#define NUM_EXCEPTIONS    		32u
#define NUM_IRQS        		24u

#define is_valid_irq(irq)		(irq < NUM_IRQS)

struct CpuExInterruptFrame {
    uint32_t ex_num;
    uint32_t error_code;
    uint32_t old_tss_esp0;
    ExecutionState state;
};

struct IrqInterruptFrame {
    uint32_t old_tss_esp0;
    ExecutionState state;
};

extern NAKED noreturn void cpu_ex0_handler(void);
extern NAKED noreturn void cpu_ex1_handler(void);
extern NAKED noreturn void cpu_ex2_handler(void);
extern NAKED noreturn void cpu_ex3_handler(void);
extern NAKED noreturn void cpu_ex4_handler(void);
extern NAKED noreturn void cpu_ex5_handler(void);
extern NAKED noreturn void cpu_ex6_handler(void);
extern NAKED noreturn void cpu_ex7_handler(void);
extern NAKED noreturn void cpu_ex8_handler(void);
extern NAKED noreturn void cpu_ex9_handler(void);
extern NAKED noreturn void cpu_ex10_handler(void);
extern NAKED noreturn void cpu_ex11_handler(void);
extern NAKED noreturn void cpu_ex12_handler(void);
extern NAKED noreturn void cpu_ex13_handler(void);
extern NAKED noreturn void cpu_ex14_handler(void);
extern NAKED noreturn void cpu_ex15_handler(void);
extern NAKED noreturn void cpu_ex16_handler(void);
extern NAKED noreturn void cpu_ex17_handler(void);
extern NAKED noreturn void cpu_ex18_handler(void);
extern NAKED noreturn void cpu_ex19_handler(void);
extern NAKED noreturn void cpu_ex20_handler(void);
extern NAKED noreturn void cpu_ex21_handler(void);
extern NAKED noreturn void cpu_ex22_handler(void);
extern NAKED noreturn void cpu_ex23_handler(void);
extern NAKED noreturn void cpu_ex24_handler(void);
extern NAKED noreturn void cpu_ex25_handler(void);
extern NAKED noreturn void cpu_ex26_handler(void);
extern NAKED noreturn void cpu_ex27_handler(void);
extern NAKED noreturn void cpu_ex28_handler(void);
extern NAKED noreturn void cpu_ex29_handler(void);
extern NAKED noreturn void cpu_ex30_handler(void);
extern NAKED noreturn void cpu_ex31_handler(void);

extern NAKED noreturn void irq0_handler(void);
extern NAKED noreturn void irq1_handler(void);
extern NAKED noreturn void irq2_handler(void);
extern NAKED noreturn void irq3_handler(void);
extern NAKED noreturn void irq4_handler(void);
extern NAKED noreturn void irq5_handler(void);
extern NAKED noreturn void irq6_handler(void);
extern NAKED noreturn void irq7_handler(void);
extern NAKED noreturn void irq8_handler(void);
extern NAKED noreturn void irq9_handler(void);
extern NAKED noreturn void irq10_handler(void);
extern NAKED noreturn void irq11_handler(void);
extern NAKED noreturn void irq12_handler(void);
extern NAKED noreturn void irq13_handler(void);
extern NAKED noreturn void irq14_handler(void);
extern NAKED noreturn void irq15_handler(void);
extern NAKED noreturn void irq16_handler(void);
extern NAKED noreturn void irq17_handler(void);
extern NAKED noreturn void irq18_handler(void);
extern NAKED noreturn void irq19_handler(void);
extern NAKED noreturn void irq20_handler(void);
extern NAKED noreturn void irq21_handler(void);
extern NAKED noreturn void irq22_handler(void);
extern NAKED noreturn void irq23_handler(void);

/// The threads that are responsible for handling an IRQ
extern tcb_t* irq_handlers[NUM_IRQS];

WARN_UNUSED int irq_register(tcb_t *thread, unsigned int irq);
WARN_UNUSED int irq_unregister(tcb_t *thread, unsigned int irq);

#endif /* INTERRUPT_H */