#ifndef INTERRUPT_H
#define INTERRUPT_H

#include <kernel/thread.h>
#include <kernel/message.h>

#define IRQ_PID_START  1
#define NUM_IRQS        16

void handleIRQ(int irqNum, tcb_t *thread, ExecutionState state);
void handleCPUException(int intNum, int errorCode, volatile tcb_t *thread, volatile ExecutionState);

/// The threads that are responsible for handling an IRQ

extern pid_t IRQHandlers[NUM_IRQS];
extern tcb_t *irqThreads[NUM_IRQS];
extern pid_t irqPorts[NUM_IRQS];

void endIRQ( int irqNum );
int registerInt( pid_t pid, int intNum );
void unregisterInt( int intNum );

#endif /* INTERRUPT_H */
