#include <kernel/mm.h>
#include <kernel/debug.h>
#include <kernel/thread.h>
#include <kernel/schedule.h>
#include <kernel/memory.h>
#include <os/syscalls.h>
#include <kernel/message.h>
#include <kernel/error.h>
#include <kernel/dlmalloc.h>

/** Attach a thread to a port's send queue. The thread will then enter
    the WAIT_FOR_RECV state until the recipient receives the message from
    the thread.

  @param tcb The thread to attach
  @paarm port The queue's port to which the thread will be attached
  @return E_OK on success. E_FAIL on failure.
*/

pem_t *pendingMessageBuffer;

int attachSendQueue(tcb_t *sender, tid_t recipientTid)
{
  tcb_t *recipient = getTcb(recipientTid);

  if(!recipient || (sender->threadState == READY
     && detachRunQueue(sender)) != E_OK )
  {
    return E_FAIL;
  }
  else if(queuePush(recipient->senderWaitQueue, getTid(sender), sender) == E_OK)
  {
    sender->threadState = WAIT_FOR_RECV;
    sender->waitTid = recipientTid;

    return E_OK;
  }

  attachRunQueue(sender);
  return E_FAIL;
}

/** Attach a thread to a sender's receiver list. The receiving thread will
    then enter the WAIT_FOR_SEND state until the sender sends a message to
    the recipient.

  @param receive The thread that is waiting to receive a message from
                 the sender.
  @paarm sender The thread that the recipient is waiting to send a message.
  @return E_OK on success. E_FAIL on failure.
*/

int attachReceiveQueue(tcb_t *recipient, tid_t senderTid)
{
  tcb_t *sender = getTcb(senderTid);

  if( recipient->threadState == READY && detachRunQueue(recipient) != E_OK)
    return E_FAIL;

  if(sender
     && queuePush(sender->receiverWaitQueue, getTid(recipient),
                     recipient) != E_OK)
  {
    attachRunQueue(recipient);
    return E_FAIL;
  }
  else
  {
    recipient->threadState = WAIT_FOR_SEND;
    recipient->waitTid = senderTid;

    return E_OK;
  }
}

/** Remove a thread from a receiver's send queue.

  @param tcb The thread to detach
  @return E_OK on success. E_FAIL on failure.
*/

int detachSendQueue(tcb_t *sender)
{
  tcb_t *recipient = getTcb(sender->waitTid);

  if(recipient && queueRemoveLast(recipient->senderWaitQueue,
                     getTid(sender), NULL) == E_OK)
  {
    sender->waitTid = NULL_TID;
    sender->threadState = PAUSED;
    return E_OK;
  }

  return E_FAIL;
}

/** Remove a thread from a sender's receiver list.

  @param recipient The thread that will be removed from the receive queue
  @return E_OK on success. E_FAIL on failure.
*/

int detachReceiveQueue(tcb_t *recipient)
{
  tcb_t *sender = getTcb(recipient->waitTid);

  if(sender && queueRemoveLast(sender->receiverWaitQueue,
                     getTid(recipient), NULL) == E_OK)
  {
    recipient->waitTid = NULL_TID;
    recipient->threadState = PAUSED;
    return E_OK;
  }

  return E_FAIL;
}

/**
  Synchronously sends a message from the current thread to the recipient thread.

  If the recipient is not ready to receive the message, then wait unless
  non-blocking.

  @param tcb The TCB of the sender.
  @param portPair The local, remote pair of ports. Upper 16-bits
         is the local port, lower 16-bits is the remote port.
  @param block Non-zero if a blocking send (i.e. wait until a recipient
         receives from the local port). Otherwise, give up if no recipients
         are available.
  @return E_OK on success. E_FAIL on failure. E_INVALID_ARG on bad argument.
          E_BLOCK if no recipient is ready to receive (and not blocking).
*/

int sendMessage(tcb_t *sender, tid_t recipientTid, int block, int call)
{
  tcb_t *recipient = getTcb(recipientTid);
  tid_t senderTid = getTid(sender);

  assert( sender != NULL );
  assert( sender->threadState == RUNNING );

  if(!recipient)
  {
    kprintf("Invalid recipient.\n");
    return E_INVALID_ARG;
  }

  /* This may cause problems if the recipient is receiving into a NULL
     or non-mapped buffer */

  // If the recipient is waiting for a message from this sender or any sender

  if(recipient->threadState == WAIT_FOR_SEND
     && (recipient->waitTid == NULL_TID
         || recipient->waitTid == senderTid))
  {
    recipient->execState.eax = (int)senderTid;
    recipient->execState.ebx = sender->execState.ebx;
    recipient->execState.ecx = sender->execState.ecx;
    recipient->execState.edx = sender->execState.edx;
    recipient->execState.esi = sender->execState.esi;
    recipient->execState.edi = sender->execState.edi;

    detachReceiveQueue(recipient);

    if(call)
      attachReceiveQueue(sender, recipientTid);

    startThread(recipient);
  }
  else if( !block )	// Recipient is not ready to receive, but we're not allowed to block, so return
  {
    kprintf("call: Non-blocking. TID: %d\tEIP: 0x%x\n", senderTid, sender->execState.eip);
    kprintf("EIP: 0x%x\n", *(dword *)(sender->execState.ebp + 4));
    return E_BLOCK;
  }
  else	// Wait until the recipient is ready to receive the message
    attachSendQueue(sender, getTid(recipient));

  return E_OK;
}

int sendExceptionMessage(tcb_t * restrict sender, tid_t recipientTid,
                         pem_t * restrict message)
{
  tcb_t *recipient = getTcb(recipientTid);

  if(!recipient)
    return E_FAIL;

  // If the recipient is waiting for a message from this sender or any sender

  if(recipient->threadState == WAIT_FOR_SEND
     && recipient->waitTid == NULL_TID)
  {
    recipient->execState.eax = (message->subject << 16) | (tid_t)NULL_TID;
    recipient->execState.ebx = message->errorCode;
    recipient->execState.ecx = message->faultAddress;

    detachReceiveQueue(recipient);
    startThread(recipient);
  }
  else  // Wait until the recipient is ready to receive the message
  {
    tid_t senderTid = getTid(sender);
    attachSendQueue(sender, recipientTid);

    pendingMessageBuffer[senderTid] = *message;
  }

  return E_OK;
}


/**
  Synchronously receives a message from a thread.

  If no message can be received, then wait for a message from sender
  unless indicated as non-blocking.

  @param recipient The recipient of the message.
  @param port     The port on which the recipient is waiting for a message.
  @param sender   The thread that the recipient expects to receive a message.
                  NULL, if receiving from any sender.
  @param block Non-zero if a blocking receive (i.e. wait until a sender
         sends to the local port). Otherwise, give up if no senders
         are available.
  @return E_OK on success. E_FAIL on failure. E_INVALID_ARG on bad argument.
          E_BLOCK if no messages are pending to be received (and non-blocking).
*/

int receiveMessage( tcb_t *recipient, tid_t senderTid, int block )
{
  tcb_t *sender = getTcb(senderTid);
  tid_t recipientTid = getTid(recipient);

  assert( recipient != NULL );
  assert( recipient->threadState == RUNNING );

  if(recipient == sender)
  {
    kprintf("Invalid port.\n");
    return E_INVALID_ARG;
  }

  if(!sender) // receive message from anyone
    sender = queueGetTail(recipient->senderWaitQueue);

  if(sender && sender->waitTid == recipientTid)
  {
    if(sender->threadState == WAIT_FOR_RECV)
    {
      recipient->execState.ecx = sender->execState.ecx;
      recipient->execState.edx = sender->execState.edx;
      recipient->execState.esi = sender->execState.esi;
      recipient->execState.edi = sender->execState.edi;
      recipient->execState.ebx = sender->execState.ebx;

      if((sender->execState.eax & 0xFFFF) == SYS_CALL_WAIT)
      {
        detachSendQueue(sender);
        attachReceiveQueue(sender, recipientTid);
      }
      else
      {
        sender->execState.eax = ESYS_OK;
        startThread(sender);
      }

      return (int)senderTid;
    }
    else if(sender->threadState == PAUSED) // sender sent an exception
    {
      pem_t *message = &pendingMessageBuffer[senderTid];

      recipient->execState.ebx = (dword)message->errorCode;
      recipient->execState.ecx = (dword)message->faultAddress;
      return (int)(NULL_TID | (message->subject << 16));
    }
  }
  else if( !block )
  {
    kprintf("receive: Non-blocking. TID: %d\tEIP: 0x%x\n", recipientTid, recipient->execState.eip);
    kprintf("EIP: 0x%x\n", *(dword *)(recipient->execState.ebp + 4));
    return E_BLOCK;
  }
  else // no one is waiting to send to this local port, so wait
    attachReceiveQueue(recipient, senderTid);

  return (int)NULL_TID;
}
