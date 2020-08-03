#ifndef MESSAGE_H
#define MESSAGE_H

#include <oslib.h>
#include <kernel/thread.h>
#include <kernel/struct.h>

struct PendingExceptionMessage
{
  unsigned char subject;
  unsigned char intNum;
  tid_t who;
  int errorCode;
  int faultAddress;
} __PACKED__;

typedef struct PendingExceptionMessage pem_t;

HOT(int sendMessage(tcb_t *sender, tid_t recipientTid, int block, int call));
HOT(int receiveMessage(tcb_t *recipient, tid_t senderTid, int block));
int sendExceptionMessage(tcb_t * restrict sender, tid_t recipientTid,
                         pem_t * restrict message);

int attachSendQueue(tcb_t *sender, tid_t recipient);
int attachReceiveQueue(tcb_t *receiver, tid_t sender);
int detachSendQueue(tcb_t *sender);
int detachReceiveQueue(tcb_t *receiver);

extern pem_t *pendingMessageBuffer;

#endif /* MESSAGE */
