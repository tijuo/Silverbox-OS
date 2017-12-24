#ifndef INTERRUPT_H
#define INTERRUPT_H

#include <kernel/thread.h>

void endIRQ( int irqNum );
int registerInt( TCB *thread, int intNum );
void unregisterInt( int intNum );

#endif /* INTERRUPT_H */
