#ifndef INTERRUPT_H
#define INTERRUPT_H

#include <kernel/thread.h>
#include <kernel/message.h>

#define TIMER_IRQ           0
#define SPURIOUS_IRQ        7
#define PAGE_FAULT_INT      14

#define NUM_IRQS        	16
#define IRQ_BITMAP_COUNT	((NUM_IRQS / sizeof(u32))+(NUM_IRQS & (sizeof(u32)-1) ? 1 : 0))
#define isValidIRQ(irq)		({ __typeof__ (irq) _irq=(irq); (_irq >= 0 && _irq < NUM_IRQS); })

void handleIRQ(int irqNum, ExecutionState *state);
void handleCPUException(int intNum, int errorCode, ExecutionState *state);

/// The threads that are responsible for handling an IRQ

extern tcb_t *irqHandlers[NUM_IRQS];
extern u32 pendingIrqBitmap[IRQ_BITMAP_COUNT];

void endIRQ( int irqNum );
int registerIrq( tcb_t *thread, int irqNum );
int unregisterIrq( int irqNum );

#endif /* INTERRUPT_H */
