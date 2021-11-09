#ifndef INTERRUPT_H
#define INTERRUPT_H

#include <kernel/thread.h>
#include <kernel/message.h>

#define PAGE_FAULT_INT      14u

#define NUM_IRQS        	24u
#define isValidIRQ(irq)		({ __typeof__ (irq) _irq=(irq); (_irq < NUM_IRQS); })

void handleIRQ(unsigned int intNum, ExecutionState *state);
void handleCPUException(unsigned int intNum, int errorCode, ExecutionState *state);

/// The threads that are responsible for handling an IRQ

extern tcb_t *irqHandlers[NUM_IRQS];

#endif /* INTERRUPT_H */
