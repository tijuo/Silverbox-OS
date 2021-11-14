#include <kernel/mm.h>
#include <kernel/debug.h>
#include <kernel/thread.h>
#include <kernel/schedule.h>
#include <kernel/memory.h>
#include <kernel/syscall.h>
#include <kernel/message.h>
#include <kernel/error.h>
#include <kernel/bits.h>

/** Attach a sending thread to a recipient's send queue. The sender will then enter
 the WAIT_FOR_RECV state until the recipient receives the message from
 the thread.

 @param sender The thread to attach.
 @param recipientTid The tid of the recipient to which the sender will be attached. (Must not be NULL_TID)
 @return E_OK on success. E_FAIL on failure.
 */

NON_NULL_PARAMS int attachSendWaitQueue(tcb_t *sender, tid_t recipientTid) {
  assert(recipientTid != NULL_TID);

  tcb_t *recipient = getTcb(recipientTid);

  listEnqueue(&recipient->senderWaitQueue, sender);
  sender->threadState = WAIT_FOR_RECV;
  sender->waitTid = recipientTid;

  return E_OK;
}

/** Attach a recipient to a sender's receive queue. The receiving thread will
 then enter the WAIT_FOR_SEND state until the sender sends a message to
 the recipient.

 @param recipient The thread that is waiting to receive a message from
 a sender.
 @paarm senderTid The tid of the thread that the recipient is waiting to send a message.
 @return E_OK on success. E_FAIL on failure.
 */

NON_NULL_PARAMS int attachReceiveWaitQueue(tcb_t *recipient, tid_t senderTid) {
  tcb_t *sender = getTcb(senderTid);

  // If sender is NULL, then the kernel sent the message. No need to attach to a wait queue

  if(sender)
    listEnqueue(&sender->receiverWaitQueue, recipient);

  recipient->threadState = WAIT_FOR_SEND;
  recipient->waitTid = senderTid;

  return E_OK;
}

/** Remove a sender from its recipient's send queue.

 @param sender The sender to be detached. (Must not be NULL)
 @return E_OK on success. E_FAIL on failure.
 */

NON_NULL_PARAMS int detachSendWaitQueue(tcb_t *sender) {
  tcb_t *recipient = getTcb(sender->waitTid);

  sender->waitTid = NULL_TID;

  if(recipient)
    listRemove(&recipient->senderWaitQueue, sender);

  return E_OK;
}

/** Remove a recipient from its sender's receive queue.

 @param recipient The recipient that will be detached. (Must not be NULL)
 @return E_OK on success. E_FAIL on failure.
 */

NON_NULL_PARAMS int detachReceiveWaitQueue(tcb_t *recipient) {
  tcb_t *sender = getTcb(recipient->waitTid);

  recipient->waitTid = NULL_TID;

  if(sender)
    listRemove(&sender->receiverWaitQueue, recipient);

  return E_OK;
}

/**
 Synchronously send a message from a sender thread to a recipient thread.

 If the recipient is not ready to receive the message, then wait unless
 non-blocking.

 @param sender The sending thread.
 @param recipientTid The TID of the recipient thread.
 @param replierTid The TID of the replier from which the sender will wait for a reply
 (ignored if sendOnly is true.)
 @param subject The subject of the sending message.
 @param sendFlags The message flags for the sent message.
 @param recvFlag The flags for the received message (ignored if sendOnly is true.)
 @param sendOnly true if the sender isn't expecting to receive a reply message.
 false if the sender expects to receive a message from a replier.
 @return E_OK on success. E_FAIL on failure. E_INVALID_ARG on bad argument.
 E_BLOCK if no recipient is ready to receive (and not blocking).
 */

// XXX: What if recipient is waiting to receive a message from TID x, but
// XXX: TID x sends an exception message to recipient? Recipient's
// XXX: receive should return with E_INTERRUPT. And any subsequent
// XXX: receive()s from that TID should result in E_INTERRUPT.
NON_NULL_PARAMS
int _sendAndReceiveMessage(tcb_t *sender, tid_t recipientTid, tid_t replierTid,
                           uint32_t subject, uint32_t sendFlags,
                           uint32_t recvFlags, bool sendOnly)
{
  tid_t senderTid = getTid(sender);
  tcb_t *recipient;
  int isKernelMessage = IS_FLAG_SET(sendFlags, MSG_KERNEL);

//  kprintf("sendMessage(): %d->%d subject %#x flags: %#x\n", senderTid, msg->target.recipient, msg->subject, msg->flags);

  if(senderTid == recipientTid) // If sender wants to call itself, then simply set the return value
    RET_MSG(E_INVALID_ARG, "Sender attempted to send a message to itself.");
  else if(senderTid == replierTid)
    RET_MSG(E_INVALID_ARG,
            "Sender attempted to receive a message from itself.");
  else if(!(recipient = getTcb(recipientTid)))
    RET_MSG(E_INVALID_ARG, "Invalid recipient.");
  else if(recipient->threadState == INACTIVE || recipient->threadState == ZOMBIE)
    RET_MSG(E_UNREACH, "Attempting to send message to inactive thread.");

  // TODO: Do not allow cycles to form in wait queues.

  // If the recipient is waiting for a message from this sender or any sender

  if(recipient->threadState == WAIT_FOR_SEND && (recipient->waitTid
      == ANY_SENDER
                                                 || (recipient->waitTid == senderTid && !isKernelMessage
                                                     && !recipient
                                                       ->waitForKernelMsg)
                                                 || (isKernelMessage && recipient
                                                       ->waitForKernelMsg))) {
    recipient->waitForKernelMsg = 0;

    //kprintf("%d is sending message to %d subject %#x flags: %#x\n", senderTid, msg->recipient, msg->subject, msg->flags);

    startThread(recipient);

    recipient->userExecState.eax = E_OK;
    recipient->userExecState.ebx = senderTid;
    recipient->userExecState.esi = subject;
    recipient->userExecState.edi = sendFlags;

    //kprintf("%d: Called %d with subject %d now waiting for response\n", getTid(sender), getTid(recipient), msg->subject);
    if(!sendOnly && IS_ERROR(receiveMessage(sender, replierTid, recvFlags)))
      RET_MSG(E_FAIL, "Unable to complete call.");

    return E_OK;
  }
  else if(IS_FLAG_SET(sendFlags, MSG_NOBLOCK)) // Recipient is not ready to receive, but we're not allowed to block, so return
    return E_BLOCK;
  else  // Wait until the recipient is ready to receive the message
  {
    //kprintf("%d: Waiting to send subj: %d to %d\n", senderTid, msg->subject, msg->recipient);

    if(IS_ERROR(removeThreadFromList(sender)))
      RET_MSG(E_FAIL, "Unable to detach sender from run queue.");

    attachSendWaitQueue(sender, getTid(recipient));

    //kprintf("Waiting until recipient is read...\n");

    sender->waitForKernelMsg = !!isKernelMessage;
    sender->userExecState.eax = E_INTERRUPT;

    // todo: Set a flag so that on receive(), it completes the sendAndReceive
    switchContext(schedule(getCurrentProcessor()), 1);

    // Does not return
  }

  RET_MSG(E_FAIL, "This should never happen.");
}

/**
 Synchronously receive a message from a thread. If no message can be received, then wait
 for a message from the sender unless indicated as non-blocking.

 @param recipient The recipient of the message.
 @param senderTid The tid of the sender that the recipient expects to receive a message.
 ANY_SENDER, if receiving from any sender.
 @param block Non-zero if a blocking receive (i.e. wait until a sender
 sends). Otherwise, give up if no senders are available.
 @param isLong non-zero if sending a message via buffer instead of via registers
 @return E_OK on success. E_FAIL on failure. E_INVALID_ARG on bad argument.
 E_BLOCK if no messages are pending to be received (and non-blocking).
 */

NON_NULL_PARAMS
int receiveMessage(tcb_t *recipient, tid_t senderTid, uint32_t flags)
{
  tcb_t *sender = getTcb(senderTid);
  tid_t recipientTid = getTid(recipient);
  int isKernelMessage = IS_FLAG_SET(flags, MSG_KERNEL);

  //kprintf("%d: receiveMessage()\n", recipientTid);

  // TODO: Do not allow cycles to form in wait queues.

  if(recipient == sender)
    RET_MSG(E_INVALID_ARG,
            "Recipient attempted to receive message from itself.");
  else if(sender
      && (sender->threadState == INACTIVE || sender->threadState == ZOMBIE))
    RET_MSG(E_UNREACH, "Attempting to receive message from inactive thread.");

  if(!sender && !isListEmpty(&recipient->senderWaitQueue)) // receive message from anyone
  {
    sender = listDequeue(&recipient->senderWaitQueue);
    senderTid = getTid(sender);
  }

  //kprintf("receiveMessage(): %d<-%d flags: %#x\n", recipientTid, senderTid, msg->flags);

  if(sender && sender->waitTid == recipientTid
     && (!isKernelMessage || (isKernelMessage && sender->waitForKernelMsg))) {
    //kprintf("%d: Receiving message...\n", recipientTid);

    startThread(sender);

    sender->waitForKernelMsg = 0;

    sender->userExecState.eax = E_OK;
    recipient->userExecState.eax = E_OK;
    recipient->userExecState.ebx = senderTid;
    recipient->userExecState.esi = sender->userExecState.esi; // sender flags
    recipient->userExecState.edi = sender->userExecState.edi;

    //kprintf("%d is receiving message from %d subject %#x flags: %#x\n", recipientTid, senderTid, msg->subject, msg->flags);

    __asm__("fxrstor %0\n" :: "m"(sender->xsaveState));

    switchContext(recipient, 0); // don't use sysexit. do an iret instead so that args are restored
    // Does not return
  }
  else if(IS_FLAG_SET(flags, MSG_NOBLOCK))
    return E_BLOCK;
  else // no one is waiting to send to this local port, so wait
  {
    //kprintf("%d: Waiting to receive from %d\n", recipientTid, msg->sender);

    if(IS_ERROR(removeThreadFromList(recipient)))
      RET_MSG(E_FAIL, "Unable to detach recipient from run queue.");

    attachReceiveWaitQueue(recipient, senderTid);

    recipient->waitForKernelMsg = !!isKernelMessage;
    recipient->userExecState.eax = E_INTERRUPT;

    // Receive will be completed when sender does a send

    switchContext(schedule(getCurrentProcessor()), 1);
  }

  RET_MSG(E_FAIL, "This should never happen.");
}
