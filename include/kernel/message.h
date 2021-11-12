#ifndef MESSAGE_H
#define MESSAGE_H

#include <oslib.h>
#include <kernel/thread.h>
#include <kernel/list.h>
#include <os/msg/kernel.h>

NON_NULL_PARAMS
int receiveMessage(tcb_t *recipient, tid_t senderTid, uint32_t flags);

NON_NULL_PARAMS
int _sendAndReceiveMessage(tcb_t *sender, tid_t recipientTid, tid_t replierTid,
                           uint32_t subject, uint32_t sendFlags, uint32_t recvFlags, bool sendOnly);

NON_NULL_PARAMS
static inline int sendMessage(tcb_t *sender, tid_t recipientTid, uint32_t subject, uint32_t flags)
{
  return _sendAndReceiveMessage(sender, recipientTid, NULL_TID, subject, flags, 0, true);
}

NON_NULL_PARAMS
static inline int sendAndReceiveMessage(tcb_t *sender, tid_t recipientTid, tid_t replierTid,
                                        uint32_t subject, uint32_t sendFlags, uint32_t recvFlags)
{
  return _sendAndReceiveMessage(sender, recipientTid, replierTid, subject, sendFlags, recvFlags, false);
}

NON_NULL_PARAMS
int attachSendWaitQueue(tcb_t *sender, tid_t recipient);

NON_NULL_PARAMS
int attachReceiveWaitQueue(tcb_t *receiver, tid_t sender);

NON_NULL_PARAMS
int detachSendWaitQueue(tcb_t *sender);

NON_NULL_PARAMS
int detachReceiveWaitQueue(tcb_t *receiver);

#endif /* MESSAGE */
