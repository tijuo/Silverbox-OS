#ifndef SIGNAL_H
#define SIGNAL_H

#include <os/signal.h>
#include <kernel/thread.h>

int sysRaise( TCB *tcb, int signal, int arg );
int sysSetSigHandler( TCB *tcb, void *handler );

#endif /* SIGNAL_H */
