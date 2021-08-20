#include <kernel/mm.h>
#include <kernel/debug.h>
#include <kernel/thread.h>
#include <kernel/schedule.h>
#include <kernel/memory.h>
#include <kernel/syscall.h>
#include <kernel/message.h>
#include <kernel/error.h>

/** Attach a sending thread to a recipient's send queue. The sender will then enter
    the WAIT_FOR_RECV state until the recipient receives the message from
    the thread.

  @param sender The thread to attach.
  @paarm recipientTid The tid of the recipient to which the sender will be attached.
  @return E_OK on success. E_FAIL on failure.
 */

int attachSendWaitQueue(tcb_t *sender, tid_t recipientTid)
{
  tcb_t *recipient = getTcb(recipientTid);

  assert(sender);

  if(!recipient)
    RET_MSG(E_FAIL, "Invalid recipient");
  else if(sender->threadState == READY && !detachRunQueue(sender))
    RET_MSG(E_FAIL, "Unable to detach thread from run queue");
  else if(!IS_ERROR(listEnqueue(&recipient->senderWaitQueue, sender)))
  {
    sender->threadState = WAIT_FOR_RECV;
    sender->waitTid = recipientTid;

    return E_OK;
  }

  if(!attachRunQueue(sender))
    panic("Unable to reattach sender to run queue.");

  RET_MSG(E_FAIL, "Unable to enqueue thread to sender wait queue.");
}

/** Attach a recipient to a sender's receive queue. The receiving thread will
    then enter the WAIT_FOR_SEND state until the sender sends a message to
    the recipient.

  @param recipient The thread that is waiting to receive a message from
                   a sender.
  @paarm senderTid The tid of the thread that the recipient is waiting to send a message.
  @return E_OK on success. E_FAIL on failure.
 */

int attachReceiveWaitQueue(tcb_t *recipient, tid_t senderTid)
{
  tcb_t *sender = getTcb(senderTid);

  assert(recipient);

  if(recipient->threadState == READY && !detachRunQueue(recipient))
    RET_MSG(E_FAIL, "Unable to detach recipient from run queue.");

  if(sender && IS_ERROR(listEnqueue(&sender->receiverWaitQueue, recipient)))
  {
    if(!attachRunQueue(sender))
      panic("Unable to reattach recipient to run queue.");

    RET_MSG(E_FAIL, "Unable to enqueue thread to recipient wait queue.");
  }
  else
  {
    recipient->threadState = WAIT_FOR_SEND;
    recipient->waitTid = senderTid;

    return E_OK;
  }
}

/** Remove a sender from its recipient's send queue.

  @param sender The sender to be detached.
  @return E_OK on success. E_FAIL on failure.
 */

int detachSendWaitQueue(tcb_t *sender)
{
  assert(sender);
  tcb_t *recipient = getTcb(sender->waitTid);

  if(recipient && !IS_ERROR(listRemove(&recipient->senderWaitQueue, sender)))
  {
    sender->waitTid = NULL_TID;
    sender->threadState = PAUSED;
    return E_OK;
  }

  RET_MSG(E_FAIL, "Unable to detach sender from sender wait queue.");
}

/** Remove a recipient from its sender's receive queue.

  @param recipient The recipient that will be detached.
  @return E_OK on success. E_FAIL on failure.
 */

int detachReceiveWaitQueue(tcb_t *recipient)
{
  assert(recipient);
  tcb_t *sender = getTcb(recipient->waitTid);

  if((sender && !IS_ERROR(listRemove(&sender->receiverWaitQueue, recipient)))
      || (!sender && recipient->waitTid == ANY_SENDER))
  {
    recipient->waitTid = NULL_TID;
    recipient->threadState = PAUSED;
    return E_OK;
  }

  RET_MSG(E_FAIL, "Unable to detach recipient from recipient wait queue.");
}

/**
  Synchronously send a message from a sender thread to a recipient thread.

  If the recipient is not ready to receive the message, then wait unless
  non-blocking.

  @param sender The sending thread.
  @param state The saved execution state of the processor while the thread was in user-mode.
  @param recipientTid The TID of the recipient thread.
  @param block Non-zero if a blocking send (i.e. wait until a recipient
         receives). Otherwise, give up if no recipients are available.
  @param call non-zero if the sender expects to receive a message from the recipient
              as a reply. 0, otherwise.
  @param isLong non-zero if sending a message via buffer instead of via registers
  @return E_OK on success. E_FAIL on failure. E_INVALID_ARG on bad argument.
          E_BLOCK if no recipient is ready to receive (and not blocking).
 */

// XXX: What if recipient is waiting to receive a message from TID x, but
// XXX: TID x sends an exception message to recipient? Recipient's
// XXX: receive should return with E_INTERRUPT. And any subsequent
// XXX: receive()s from that TID should result in E_INTERRUPT.

int sendMessage(tcb_t *sender, msg_t *msg, msg_t *callMsg)
{
  assert(msg);

  tid_t senderTid = getTid(sender);
  tcb_t *recipient;
  int isKernelMessage = (msg->flags & MSG_KERNEL) == MSG_KERNEL;
  int isCall = (msg->flags & MSG_CALL);

  if(isCall)
    assert(callMsg);

  assert(sender);

  //kprintf("sendMessage(): %d->%d subject 0x%x flags: 0x%x\n", senderTid, msg->recipient, msg->subject, msg->flags);

  if(senderTid == msg->recipient) // If sender wants to call itself, then simply set the return value
  {
    if(isCall)
    {
      msg->bytesTransferred = msg->bufferLen;
      return E_OK;
    }
    else
      RET_MSG(E_INVALID_ARG, "Sender attempted to send a message to itself.");
  }

  // TODO: Do not allow cycles to form in wait queues.

  if(!(recipient=getTcb(msg->recipient)))
    RET_MSG(E_INVALID_ARG, "Invalid recipient.");

  if(recipient->threadState == INACTIVE || recipient->threadState == ZOMBIE)
  {
    kprintf("%d is inactive!\n", getTid(recipient));
    RET_MSG(E_FAIL, "Attempting to send message to inactive thread.");
  }

  msg->sender = senderTid;

  // If the recipient is waiting for a message from this sender or any sender

  if(recipient->threadState == WAIT_FOR_SEND
      && (recipient->waitTid == ANY_SENDER
          || (recipient->waitTid == senderTid && !isKernelMessage && !recipient->waitForKernelMsg)
          || (isKernelMessage && recipient->waitForKernelMsg)))
  {
    //kprintf("%d: sending subj: %d message to %d\n", senderTid, msg->subject, msg->recipient);

    if(IS_ERROR(detachReceiveWaitQueue(recipient)))
      RET_MSG(E_FAIL, "Unable to detach recipient from recipient wait queue.");
    /*
    else if(isCall && IS_ERROR(attachReceiveWaitQueue(sender, getTid(recipient))))
      RET_MSG(E_FAIL, "Unable to attach sender to sender wait queue.");
    */
    else if(IS_ERROR(startThread(recipient)))
      RET_MSG(E_FAIL, "Unable to restart recipient.");

    msg_t recvMsg;

    if(IS_ERROR(peekVirt((addr_t)recipient->pendingMessage, sizeof recvMsg, &recvMsg, recipient->rootPageMap)))
      RET_MSG(E_FAIL, "Unable to read recipient's message struct.");

    size_t bytesTransferred = MIN(recvMsg.buffer ? recvMsg.bufferLen : 0, msg->bufferLen);

    if(bytesTransferred)
    {
      if(IS_ERROR(pokeVirt((addr_t)recvMsg.buffer, bytesTransferred, msg->buffer, recipient->rootPageMap)))
        RET_MSG(E_FAIL, "Unable to write to recipient's buffer.");
    }

    recvMsg.sender = senderTid;
    recvMsg.subject = msg->subject;

    msg->recipient = getTid(recipient);
    recvMsg.flags = msg->flags;
    recvMsg.bytesTransferred = bytesTransferred;
    msg->bytesTransferred = bytesTransferred;

    if(IS_ERROR(pokeVirt((addr_t)recipient->pendingMessage, sizeof *recipient->pendingMessage, &recvMsg, recipient->rootPageMap)))
      RET_MSG(E_FAIL, "Unable to write to recipient's message struct.");

    recipient->pendingMessage = NULL;
    recipient->waitForKernelMsg = 0;

    //kprintf("%d is sending message to %d subject 0x%x flags: 0x%x\n", senderTid, msg->recipient, msg->subject, msg->flags);

    if(isCall)
    {
      assert(callMsg);

      callMsg->sender = getTid(recipient);
      callMsg->flags = (msg->flags & ~(MSG_NOBLOCK | MSG_KERNEL));
      //kprintf("%d: Called %d with subject %d now waiting for response\n", getTid(sender), getTid(recipient), msg->subject);
      if(IS_ERROR(receiveMessage(sender, callMsg)))
        RET_MSG(E_FAIL, "Unable to complete call.");
      else
      {
        //kprintf("%d Call complete\n", getTid(sender));
        return E_OK;
      }
    }
    else
      return E_OK;
  }
  else if( msg->flags & MSG_NOBLOCK ) // Recipient is not ready to receive, but we're not allowed to block, so return
    return E_BLOCK;
  else  // Wait until the recipient is ready to receive the message
  {
    //kprintf("%d: Waiting to send subj: %d to %d\n", senderTid, msg->subject, msg->recipient);

    if(IS_ERROR(attachSendWaitQueue(sender, getTid(recipient))))
      RET_MSG(E_FAIL, "Unable to attach sender to sender wait queue.");

    //kprintf("Waiting until recipient is read...\n");

    sender->pendingMessage = msg;
    sender->responseMessage = isCall ? callMsg : NULL;
    sender->waitForKernelMsg = !!isKernelMessage;

    int result = saveAndSwitchContext(NULL);
/*
    if(result == 0)
    {
      if(isCall)
      {
        assert(callMsg);

        callMsg->sender = getTid(recipient);
        callMsg->flags = (msg->flags & ~(MSG_NOBLOCK | MSG_KERNEL));
        kprintf("%d: Called %d with subject %d now waiting for response.\n", getTid(sender), getTid(recipient), msg->subject);

        if(IS_ERROR(receiveMessage(sender, callMsg)))
          RET_MSG(E_FAIL, "Unable to complete call.");
        else
        {
          kprintf("%d: Call completed.\n", getTid(sender));
          return E_OK;
        }
      }
      else
        return E_OK;
    }
    else
      return E_FAIL;
*/
    return result == 0 ? E_OK : E_FAIL;
  }
}

/**
  Send a message originating from the kernel to a recipient.

  If the recipient is not ready to receive the message, then save the message
  in the pending message buffer, until the recipient is ready.

  @param sender The sender thread.
  @param recipientTid The TID of the recipient thread.
  @param message The kernel message to be sent.
  @return E_OK on success. E_FAIL on failure. E_INVALID_ARG on bad argument.
          E_BLOCK if no recipient is ready to receive (and not blocking).
 */

int sendKernelMessage(tid_t recipientTid, unsigned char msgSubject, void *data, size_t dataLen)
{
  msg_t exceptionMessage = {
      .sender = 0,
      .recipient = recipientTid,
      .subject = msgSubject,
      .flags = MSG_KERNEL | MSG_CALL,
      .buffer = data,
      .bufferLen = dataLen
  };

  msg_t responseMessage = {
    .sender = recipientTid,
    .buffer = NULL,
    .bufferLen = 0,
    .flags = 0,
  };

  return sendMessage(currentThread, &exceptionMessage, &responseMessage);
/*
  if(result == 0)
  {
    if((responseMessage.flags & MSG_SYSTEM) && responseMessage.subject == RESPONSE_OK)
      kprintf("Exception handler thread responded to kernel message successfully.\n");
    else if(!(responseMessage.flags & MSG_SYSTEM))
      kprintf("Exception handler did not respond correctly: 0x%x.\n", responseMessage.flags);
    else
      kprintf("Exception handler thread was unable to handle kernel message\n");
  }
  else
    kprintf("Unable to send kernel message to exception handler thread.\n");
*/
//  return result;
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

int receiveMessage( tcb_t *recipient, msg_t *msg )
{
  tcb_t *sender = getTcb(msg->sender);
  tid_t recipientTid = getTid(recipient);
  int isKernelMessage = (msg->flags & MSG_KERNEL);

  assert(msg);

  assert( recipient != NULL );
  //kprintf("%d: receiveMessage()\n", recipientTid);

  // TODO: Do not allow cycles to form in wait queues.

  if(recipient == sender)
    RET_MSG(E_INVALID_ARG, "Recipient attempted to receive message from itself.");
  else if(sender && (sender->threadState == INACTIVE || sender->threadState == ZOMBIE))
    RET_MSG(E_FAIL, "Attempting to receive message from inactive thread.");

  if(!sender && !isListEmpty(&recipient->senderWaitQueue)) // receive message from anyone
  {
    sender = listDequeue(&recipient->senderWaitQueue);
    msg->sender = getTid(sender);
  }

  msg->recipient = recipientTid;

  //kprintf("receiveMessage(): %d<-%d flags: 0x%x\n", recipientTid, senderTid, msg->flags);

  if(sender && sender->waitTid == recipientTid
     && (!isKernelMessage || (isKernelMessage && sender->waitForKernelMsg)))
  {
    if(sender->threadState == WAIT_FOR_RECV)
    {
      //kprintf("%d: Receiving message...\n", recipientTid);
      if(IS_ERROR(detachSendWaitQueue(sender)))
        RET_MSG(E_FAIL, "Unable to detach sender from sender wait queue.");

      msg_t sendMsg;

      if(IS_ERROR(peekVirt((addr_t)sender->pendingMessage, sizeof sendMsg, &sendMsg, sender->rootPageMap)))
        RET_MSG(E_FAIL, "Unable to read sender's message struct.");

      size_t bytesTransferred = MIN(sendMsg.buffer ? sendMsg.bufferLen : 0, msg->bufferLen);

      if(bytesTransferred)
      {
        if(IS_ERROR(peekVirt((addr_t)sendMsg.buffer, bytesTransferred, msg->buffer, sender->rootPageMap)))
          RET_MSG(E_FAIL, "Unable to read from sender's buffer.");
      }

      sendMsg.recipient = recipientTid;
      msg->sender = sendMsg.sender;
      msg->subject = sendMsg.subject;
      msg->flags |= sendMsg.flags;

      sendMsg.bytesTransferred = bytesTransferred;
      msg->bytesTransferred = bytesTransferred;

      if(IS_ERROR(pokeVirt((addr_t)sender->pendingMessage, sizeof *sender->pendingMessage, &sendMsg, sender->rootPageMap)))
        RET_MSG(E_FAIL, "Unable to write to sender's message struct.");

      if(sendMsg.flags & MSG_CALL)
      {
        if(IS_ERROR(attachReceiveWaitQueue(sender, recipientTid)))
          RET_MSG(E_FAIL, "Unable to attach sender to recipient wait queue.");

        sender->pendingMessage = sender->responseMessage;
      }
      else if(IS_ERROR(startThread(sender)))
        RET_MSG(E_FAIL, "Unable to restart sender.");
      else
        sender->pendingMessage = NULL;

      sender->responseMessage =  NULL;
      sender->waitForKernelMsg = 0;

      //kprintf("%d is receiving message from %d subject 0x%x flags: 0x%x\n", recipientTid, senderTid, msg->subject, msg->flags);

      return E_OK;
    }
    else
      RET_MSG(E_FAIL, "Sender was in an invalid state");
  }
  else if(msg->flags & MSG_NOBLOCK)
    return E_BLOCK;
  else // no one is waiting to send to this local port, so wait
  {
    //kprintf("%d: Waiting to receive from %d\n", recipientTid, msg->sender);

    if(IS_ERROR(attachReceiveWaitQueue(recipient, msg->sender)))
      RET_MSG(E_FAIL, "Unable to attach recipient to recipient wait queue.");

    recipient->pendingMessage = msg;
    recipient->waitForKernelMsg = !!isKernelMessage;

    return saveAndSwitchContext(NULL) != 0 ? E_FAIL : E_OK;
  }
}
