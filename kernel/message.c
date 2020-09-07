#include <kernel/mm.h>
#include <kernel/debug.h>
#include <kernel/thread.h>
#include <kernel/schedule.h>
#include <kernel/memory.h>
#include <os/syscalls.h>
#include <kernel/message.h>
#include <kernel/error.h>

pem_t pendingMessageBuffer[MAX_THREADS] __ALIGNED__(16);

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
    RET_MSG(E_FAIL, "Invalid recipient")
  else if(sender->threadState == READY && !detachRunQueue(sender))
    RET_MSG(E_FAIL, "Unable to detach thread from run queue")
  else if(!IS_ERROR(queueEnqueue(recipient->senderWaitQueue, getTid(sender), sender)))
  {
    sender->threadState = WAIT_FOR_RECV;
    sender->waitTid = recipientTid;

    return E_OK;
  }

  if(!attachRunQueue(sender))
    panic("Unable to reattach sender to run queue.");

  RET_MSG(E_FAIL, "Unable to enqueue thread to sender wait queue.")
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
    RET_MSG(E_FAIL, "Unable to detach recipient from run queue.")

  if(sender && IS_ERROR(queueEnqueue(sender->receiverWaitQueue, getTid(recipient),
                        recipient)))
  {
    if(!attachRunQueue(sender))
      panic("Unable to reattach recipient to run queue.");

    RET_MSG(E_FAIL, "Unable to enqueue thread to recipient wait queue.")
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

  if(recipient && !IS_ERROR(queueRemoveLast(recipient->senderWaitQueue,
                     getTid(sender), NULL)))
  {
    sender->waitTid = NULL_TID;
    sender->threadState = PAUSED;
    return E_OK;
  }

  RET_MSG(E_FAIL, "Unable to detach sender from sender wait queue.")
}

/** Remove a recipient from its sender's receive queue.

  @param recipient The recipient that will be detached.
  @return E_OK on success. E_FAIL on failure.
*/

int detachReceiveWaitQueue(tcb_t *recipient)
{
  assert(recipient);
  tcb_t *sender = getTcb(recipient->waitTid);

  if((sender && !IS_ERROR(queueRemoveLast(sender->receiverWaitQueue,
                     getTid(recipient), NULL)))
     || (!sender && recipient->waitTid == ANY_SENDER))
  {
    recipient->waitTid = NULL_TID;
    recipient->threadState = PAUSED;
    return E_OK;
  }

  RET_MSG(E_FAIL, "Unable to detach recipient from recipient wait queue.")
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
  @return E_OK on success. E_FAIL on failure. E_INVALID_ARG on bad argument.
          E_BLOCK if no recipient is ready to receive (and not blocking).
*/

int sendMessage(tcb_t *sender, ExecutionState *state, tid_t recipientTid, int block, int call)
{
  tid_t senderTid = getTid(sender);
  tcb_t *recipient;

  if(senderTid == recipientTid) // If sender wants to call itself, then simply set the return value
  {
    if(call)
      return (int)senderTid;
    else
      RET_MSG(E_INVALID_ARG, "Sender attempted to send a message to itself.")
  }

  if(!(recipient=getTcb(recipientTid)))
    RET_MSG(E_INVALID_ARG, "Invalid recipient.")

  if(recipient->threadState == INACTIVE)
    RET_MSG(E_INVALID_ARG, "Attempting to send message to inactive thread.")

  // If the recipient is waiting for a message from this sender or any sender

  if(recipient->threadState == WAIT_FOR_SEND
     && (recipient->waitTid == ANY_SENDER
         || recipient->waitTid == senderTid))
  {
    if(IS_ERROR(detachReceiveWaitQueue(recipient)))
      RET_MSG(E_FAIL, "Unable to detach recipient from recipient wait queue.")
    else if(call && IS_ERROR(attachReceiveWaitQueue(sender, recipientTid)))
      RET_MSG(E_FAIL, "Unable to attach sender to sender wait queue.")
    else if(IS_ERROR(startThread(recipient)))
      RET_MSG(E_FAIL, "Unable to retart recipient.")

    recipient->execState.eax = (((dword)senderTid << SYSCALL_TID_OFFSET)
        | (state->eax & SYSCALL_SUBJ_MASK) | ESYS_OK);
    recipient->execState.ebx = state->ebx;
    recipient->execState.ecx = state->ecx;
    recipient->execState.edx = state->edx;
    recipient->execState.esi = state->esi;
    recipient->execState.edi = state->edi;

    return E_OK;
  }
  else if( !block )	// Recipient is not ready to receive, but we're not allowed to block, so return
    return E_BLOCK;
  else	// Wait until the recipient is ready to receive the message
  {
    if(IS_ERROR(attachSendWaitQueue(sender, getTid(recipient))))
      RET_MSG(E_FAIL, "Unable to attach sender to sender wait queue.")

    assert(sender->threadState != READY);
    assert(sender->threadState != RUNNING);
  }

  return E_OK;
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

int sendExceptionMessage(tcb_t * sender, tid_t recipientTid,
                         pem_t * message)
{
  tcb_t *recipient = getTcb(recipientTid);
  tid_t senderTid = getTid(sender);

  if(!recipient)
    RET_MSG(E_INVALID_ARG, "Invalid recipient.")

  // If the recipient is waiting for a message from this sender or any sender

  if(recipient->threadState == WAIT_FOR_SEND
     && (recipient->waitTid == ANY_SENDER || recipient->waitTid == senderTid))
  {
    if(IS_ERROR(detachReceiveWaitQueue(recipient)))
      RET_MSG(E_FAIL, "Unable to detach recipient from recipient wait queue.")
    else if IS_ERROR(startThread(recipient))
      RET_MSG(E_FAIL, "Unable to restart recipient.")

    recipient->execState.eax = (dword)(((dword)KERNEL_TID << SYSCALL_TID_OFFSET)
        | ((dword)message->subject << SYSCALL_SUBJ_OFFSET) | ESYS_OK);
    recipient->execState.ebx = (dword)message->who;
    recipient->execState.ecx = (dword)message->errorCode;
    recipient->execState.edx = (dword)message->faultAddress;
    recipient->execState.esi = (dword)message->intNum;
  }
  else  // Wait until the recipient is ready to receive the message
  {
    if(IS_ERROR(attachSendWaitQueue(sender, recipientTid)))
      RET_MSG(E_FAIL, "Unable to attach sender to sender wait queue.")

    pendingMessageBuffer[senderTid] = *message;
  }

  sender->threadState = PAUSED;

  return E_OK;
}


/**
  Synchronously receive a message from a thread. If no message can be received, then wait
  for a message from the sender unless indicated as non-blocking.

  @param recipient The recipient of the message.
  @param senderTid The tid of the sender that the recipient expects to receive a message.
                   ANY_SENDER, if receiving from any sender.
  @param block Non-zero if a blocking receive (i.e. wait until a sender
         sends). Otherwise, give up if no senders are available.
  @return E_OK on success. E_FAIL on failure. E_INVALID_ARG on bad argument.
          E_BLOCK if no messages are pending to be received (and non-blocking).
*/

int receiveMessage( tcb_t *recipient, ExecutionState *state, tid_t senderTid, int block )
{
  tcb_t *sender = getTcb(senderTid);
  tid_t recipientTid = getTid(recipient);

  assert( recipient != NULL );
  assert( recipient->threadState == RUNNING );

  if(recipient == sender)
    RET_MSG(E_INVALID_ARG, "Recipient attempted to receive message from itself.")

  if(!sender && !isQueueEmpty(recipient->senderWaitQueue)) // receive message from anyone
  {
    sender = queueGetTail(recipient->senderWaitQueue);
    senderTid = getTid(sender);
  }

  if(sender && sender->waitTid == recipientTid)
  {
    if(sender->threadState == WAIT_FOR_RECV)
    {
      state->eax = ((dword)senderTid << SYSCALL_TID_OFFSET)
          | (sender->execState.eax & SYSCALL_SUBJ_MASK);
      state->ebx = sender->execState.ebx;
      state->ecx = sender->execState.ecx;
      state->edx = sender->execState.edx;
      state->esi = sender->execState.esi;
      state->edi = sender->execState.edi;

      if(IS_ERROR(detachSendWaitQueue(sender)))
        RET_MSG(E_FAIL, "Unablo to attach sender to sender wait queue.")

      if((sender->execState.eax & SYSCALL_CALL_MASK) == SYS_CALL_WAIT)
      {
        if(IS_ERROR(attachReceiveWaitQueue(sender, recipientTid)))
          RET_MSG(E_FAIL, "Unable to attach sender to recipient wait queue.")
      }
      else if(IS_ERROR(startThread(sender)))
        RET_MSG(E_FAIL, "Unable to restart sender.")
    }
    else if(sender->threadState == PAUSED) // sender sent an exception/exit message
    {
      pem_t *message = &pendingMessageBuffer[senderTid];

      state->eax = (dword)(((dword)KERNEL_TID << SYSCALL_TID_OFFSET)
          | ((dword)message->subject << SYSCALL_SUBJ_OFFSET));
      state->ebx = (dword)message->who;
      state->ecx = (dword)message->errorCode;
      state->edx = (dword)message->faultAddress;
      state->esi = (dword)message->intNum;

      if(IS_ERROR(detachSendWaitQueue(sender)))
        RET_MSG(E_FAIL, "Unable to detach sender from send wait queue.")
    }
    else
      RET_MSG(E_FAIL, "Sender was in an invalid state")

      return E_OK;
  }
  else if(!block)
  {
/*
    kprintf("receive: Non-blocking. TID: %d\tEIP: 0x%x\n", recipientTid, state->eip);
    kprintf("Return EIP: 0x%x\n", *(dword *)(state->ebp + 4));
*/
    return E_BLOCK;
  }
  else // no one is waiting to send to this local port, so wait
  {
    if(IS_ERROR(attachReceiveWaitQueue(recipient, senderTid)))
      RET_MSG(E_FAIL, "Unable to attach recipient to recipient wait queue.")
  }

  return E_OK;
}
