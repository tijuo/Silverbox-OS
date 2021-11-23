#ifndef INTERRUPT_H
#define INTERRUPT_H

#include <kernel/thread.h>
#include <kernel/message.h>
#include <stdnoreturn.h>
#include <util.h>

#define PAGE_FAULT_INT    14u

#define NUM_EXCEPTIONS    32u
#define NUM_IRQS        	24u
#define isValidIRQ(irq)		({ __typeof__ (irq) _irq=(irq); (_irq < NUM_IRQS); })

extern NAKED noreturn void cpuEx0Handler(void);
extern NAKED noreturn void cpuEx1Handler(void);
extern NAKED noreturn void cpuEx2Handler(void);
extern NAKED noreturn void cpuEx3Handler(void);
extern NAKED noreturn void cpuEx4Handler(void);
extern NAKED noreturn void cpuEx5Handler(void);
extern NAKED noreturn void cpuEx6Handler(void);
extern NAKED noreturn void cpuEx7Handler(void);
extern NAKED noreturn void cpuEx8Handler(void);
extern NAKED noreturn void cpuEx9Handler(void);
extern NAKED noreturn void cpuEx10Handler(void);
extern NAKED noreturn void cpuEx11Handler(void);
extern NAKED noreturn void cpuEx12Handler(void);
extern NAKED noreturn void cpuEx13Handler(void);
extern NAKED noreturn void cpuEx14Handler(void);
extern NAKED noreturn void cpuEx15Handler(void);
extern NAKED noreturn void cpuEx16Handler(void);
extern NAKED noreturn void cpuEx17Handler(void);
extern NAKED noreturn void cpuEx18Handler(void);
extern NAKED noreturn void cpuEx19Handler(void);
extern NAKED noreturn void cpuEx20Handler(void);
extern NAKED noreturn void cpuEx21Handler(void);
extern NAKED noreturn void cpuEx22Handler(void);
extern NAKED noreturn void cpuEx23Handler(void);
extern NAKED noreturn void cpuEx24Handler(void);
extern NAKED noreturn void cpuEx25Handler(void);
extern NAKED noreturn void cpuEx26Handler(void);
extern NAKED noreturn void cpuEx27Handler(void);
extern NAKED noreturn void cpuEx28Handler(void);
extern NAKED noreturn void cpuEx29Handler(void);
extern NAKED noreturn void cpuEx30Handler(void);
extern NAKED noreturn void cpuEx31Handler(void);

extern NAKED noreturn void irq0Handler(void);
extern NAKED noreturn void irq1Handler(void);
extern NAKED noreturn void irq2Handler(void);
extern NAKED noreturn void irq3Handler(void);
extern NAKED noreturn void irq4Handler(void);
extern NAKED noreturn void irq5Handler(void);
extern NAKED noreturn void irq6Handler(void);
extern NAKED noreturn void irq7Handler(void);
extern NAKED noreturn void irq8Handler(void);
extern NAKED noreturn void irq9Handler(void);
extern NAKED noreturn void irq10Handler(void);
extern NAKED noreturn void irq11Handler(void);
extern NAKED noreturn void irq12Handler(void);
extern NAKED noreturn void irq13Handler(void);
extern NAKED noreturn void irq14Handler(void);
extern NAKED noreturn void irq15Handler(void);
extern NAKED noreturn void irq16Handler(void);
extern NAKED noreturn void irq17Handler(void);
extern NAKED noreturn void irq18Handler(void);
extern NAKED noreturn void irq19Handler(void);
extern NAKED noreturn void irq20Handler(void);
extern NAKED noreturn void irq21Handler(void);
extern NAKED noreturn void irq22Handler(void);
extern NAKED noreturn void irq23Handler(void);

/// The threads that are responsible for handling an IRQ

extern tcb_t *irqHandlers[NUM_IRQS];

#endif /* INTERRUPT_H */
