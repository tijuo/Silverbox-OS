#ifndef MESSAGE_H
#define MESSAGE_H

#include <oslib.h>
#include <kernel/thread.h>
#include <kernel/list.h>
#include <os/msg/kernel.h>

int receiveMessage(tcb_t *recipient, msg_t *msg);
int sendMessage(tcb_t *sender, msg_t *msg, msg_t *callMsg);
int sendKernelMessage(tid_t recipientTid, unsigned char msgSubject, void *data, size_t dataLen);

int attachSendWaitQueue(tcb_t *sender, tid_t recipient);
int attachReceiveWaitQueue(tcb_t *receiver, tid_t sender);
int detachSendWaitQueue(tcb_t *sender);
int detachReceiveWaitQueue(tcb_t *receiver);

#endif /* MESSAGE */
