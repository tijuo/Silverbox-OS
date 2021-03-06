#ifndef MESSAGE_H
#define MESSAGE_H

#include <oslib.h>
#include <kernel/thread.h>
#include <kernel/list.h>

HOT(int sendMessage(tcb_t *sender, ExecutionState *state, tid_t recipientTid, int block, int call));
HOT(int receiveMessage(tcb_t *recipient, ExecutionState *state, tid_t senderTid, int block));
int sendExceptionMessage(tcb_t * restrict sender, tid_t recipientTid,
                         pem_t * restrict message);

int attachSendWaitQueue(tcb_t *sender, tid_t recipient);
int attachReceiveWaitQueue(tcb_t *receiver, tid_t sender);
int detachSendWaitQueue(tcb_t *sender);
int detachReceiveWaitQueue(tcb_t *receiver);

#endif /* MESSAGE */
