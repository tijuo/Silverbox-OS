#ifndef MESSAGE_H
#define MESSAGE_H

#include <oslib.h>
#include <kernel/thread.h>
#include <kernel/list.h>
#include <os/msg/kernel.h>

int receiveMessage(tcb_t *recipient, tid_t senderTid, uint32_t flags);
int _sendAndReceiveMessage(tcb_t *sender, tid_t recipientTid, tid_t replierTid,
                           uint32_t subject, uint32_t sendFlags, uint32_t recvFlags, bool sendOnly);

static inline int sendMessage(tcb_t *sender, tid_t recipientTid, uint32_t subject, uint32_t flags)
{
  return _sendAndReceiveMessage(sender, recipientTid, NULL_TID, subject, flags, 0, true);
}

static inline int sendAndReceiveMessage(tcb_t *sender, tid_t recipientTid, tid_t replierTid,
                                        uint32_t subject, uint32_t sendFlags, uint32_t recvFlags)
{
  return _sendAndReceiveMessage(sender, recipientTid, replierTid, subject, sendFlags, recvFlags, false);
}

int attachSendWaitQueue(tcb_t *sender, tid_t recipient);
int attachReceiveWaitQueue(tcb_t *receiver, tid_t sender);
int detachSendWaitQueue(tcb_t *sender);
int detachReceiveWaitQueue(tcb_t *receiver);

#endif /* MESSAGE */
