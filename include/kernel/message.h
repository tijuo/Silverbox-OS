#ifndef MESSAGE_H
#define MESSAGE_H

#include <oslib.h>
#include <kernel/thread.h>

HOT(int sendMessage( TCB *tcb, tid_t recipient, void *msg, int timeout ));
HOT(int receiveMessage( TCB *tcb, tid_t sender, void *buf, int timeout ));

#endif /* MESSAGE */
