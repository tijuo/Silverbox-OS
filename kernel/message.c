#include <kernel/mm.h>
#include <kernel/debug.h>
#include <kernel/thread.h>
#include <kernel/schedule.h>
#include <kernel/memory.h>
#include <os/syscalls.h>
#include <kernel/message.h>
#include <kernel/error.h>
#include <kernel/dlmalloc.h>

static pid_t getNewPID(void);

tree_t portTree;
static pid_t lastPID=0;

/** Attach a thread to a port's send queue. The thread will then enter
    the WAIT_FOR_RECV state until the recipient receives the message from
    the thread.

  @param tcb The thread to attach
  @paarm port The queue's port to which the thread will be attached
  @return E_OK on success. E_FAIL on failure.
*/

int attachSendQueue(tcb_t *sender, port_t *port)
{
  if( !port || (sender->threadState == READY && detachRunQueue(sender)) != E_OK )
    return E_FAIL;

  if(queueEnqueue(&port->senderWaitQueue, sender->tid, sender) != E_OK)
  {
    attachRunQueue(sender);
    return E_FAIL;
  }
  else
  {
    sender->threadState = WAIT_FOR_RECV;
    sender->wait.port = port;

    return E_OK;
  }
}

/** Attach a thread to a sender's receiver list. The receiving thread will
    then enter the WAIT_FOR_SEND state until the sender sends a message to
    the recipient.

  @param receive The thread that is waiting to receive a message from
                 the sender.
  @paarm sender The thread that the recipient is waiting to send a message.
  @return E_OK on success. E_FAIL on failure.
*/

int attachReceiveQueue(tcb_t *recipient, tcb_t *sender)
{
  if( recipient->threadState == READY && detachRunQueue(recipient) != E_OK)
    return E_FAIL;

  if(sender
     && queueEnqueue(&sender->receiverWaitQueue, recipient->tid,
                     recipient) != E_OK)
  {
    attachRunQueue(recipient);
    return E_FAIL;
  }
  else
  {
    recipient->threadState = WAIT_FOR_SEND;
    recipient->wait.thread = sender;

    return E_OK;
  }
}

/** Remove a thread from a port's send queue. The thread will then be
    reattached to the run queue.

  @param tcb The thread to detach
  @return E_OK on success. E_FAIL on failure.
*/

int detachSendQueue(tcb_t *sender)
{
  if(queueRemoveLast(&sender->wait.port->senderWaitQueue,
                     sender->tid, NULL) != E_OK)
  {
    return E_FAIL;
  }
  else
  {
    sender->wait.port = NULL;
    sender->threadState = READY;

    return attachRunQueue(sender) ? E_OK : E_FAIL;
  }
}

/** Remove a thread from a sender's receiver list. The recipient will
    then be reattached to the run queue.

  @param recipient The thread that will be removed from the receive queue
  @return E_OK on success. E_FAIL on failure.
*/

int detachReceiveQueue(tcb_t *recipient)
{
  if(queueRemoveLast(&recipient->wait.thread->receiverWaitQueue,
                     recipient->tid, NULL) != E_OK)
  {
    return E_FAIL;
  }
  else
  {
    recipient->wait.thread = NULL;
    recipient->threadState = READY;

    return attachRunQueue(recipient) ? E_OK : E_FAIL;
  }
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

int sendMessage(tcb_t *sender, pid_t remotePid, int block)
{
  tcb_t *recipient;
  port_t *remotePort = getPort(remotePid);

  assert( sender != NULL );
  assert( sender->threadState == RUNNING );

  if(remotePort == NULL || sender == remotePort->owner)
  {
    kprintf("Invalid port.\n");
    return E_INVALID_ARG;
  }

  recipient = remotePort->owner;

  /* This may cause problems if the recipient is receiving into a NULL
     or non-mapped buffer */

  // If the recipient is waiting for a message from this sender or any sender

  if(recipient->threadState == WAIT_FOR_SEND
     && (recipient->wait.thread == NULL
         || recipient->wait.thread == sender))
  {
    recipient->execState.eax = (int)sender->tid;
    recipient->execState.ebx = sender->execState.ebx;
    recipient->execState.ecx = sender->execState.ecx;
    recipient->execState.edx = sender->execState.edx;
    recipient->execState.esi = sender->execState.esi;
    recipient->execState.edi = sender->execState.edi;

    detachReceiveQueue(recipient);
    startThread(recipient);
  }
  else if( !block )	// Recipient is not ready to receive, but we're not allowed to block, so return
  {
    kprintf("send: Non-blocking. TID: %d\tEIP: 0x%x\n", sender->tid, sender->execState.eip);
    kprintf("EIP: 0x%x\n", *(dword *)(sender->execState.ebp + 4));
    return E_BLOCK;
  }
  else	// Wait until the recipient is ready to receive the message
    attachSendQueue(sender, remotePort);

  return E_OK;
}

int sendExceptionMessage(tcb_t *sender, pid_t remotePid, int args[5])
{
  port_t *remotePort = getPort(remotePid);

  if(remotePort == NULL)
    return E_FAIL;

  tcb_t *recipient = remotePort->owner;

  /* This may cause problems if the recipient is receiving into a NULL
     or non-mapped buffer */

  // If the recipient is waiting for a message from this sender or any sender

  if(recipient->threadState == WAIT_FOR_SEND
     && (recipient->wait.thread == NULL
         || recipient->wait.thread == sender))
  {
    recipient->execState.eax = (int)NULL_TID;
    recipient->execState.ebx = args[0];
    recipient->execState.ecx = args[1];
    recipient->execState.edx = args[2];
    recipient->execState.esi = args[3];
    recipient->execState.edi = args[4];

    detachReceiveQueue(recipient);
    startThread(recipient);
  }
  else  // Wait until the recipient is ready to receive the message
  {
    attachSendQueue(sender, remotePort);

    for(int i=0; i < 5; i++)
      sender->messageBuffer[i] = args[i];
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

int receiveMessage( tcb_t *recipient, pid_t localPort, tid_t senderTid, int block )
{
  tcb_t *sender = getTcb(senderTid);
  port_t *port = getPort(localPort);

  assert( recipient != NULL );
  assert( recipient->threadState == RUNNING );

  if(!port || (recipient == sender))
  {
    kprintf("Invalid port.\n");
    return E_INVALID_ARG;
  }
  else if(port->owner != recipient)
  {
    kprintf("Thread doesn't own local port.\n");
    return E_FAIL;
  }

  if(!sender) // receive message from anyone
    sender = queueGetTail(&port->senderWaitQueue);

  if(sender && sender->wait.port == port)
  {
    if(sender->threadState == WAIT_FOR_RECV)
    {
      recipient->execState.ecx = sender->execState.ecx;
      recipient->execState.edx = sender->execState.edx;
      recipient->execState.esi = sender->execState.esi;
      recipient->execState.edi = sender->execState.edi;
      recipient->execState.ebx = sender->execState.ebx;
      sender->execState.eax = ESYS_OK;
      startThread(sender);
      return (int)sender->tid;
    }
    else if(sender->threadState == PAUSED) // sender sent an exception
    {
      recipient->execState.ebx = sender->messageBuffer[0];
      recipient->execState.ecx = sender->messageBuffer[1];
      recipient->execState.edx = sender->messageBuffer[2];
      recipient->execState.esi = sender->messageBuffer[3];
      recipient->execState.edi = sender->messageBuffer[4];
      return (int)NULL_TID;
    }
  }
  else if( !block )
  {
    kprintf("receive: Non-blocking. TID: %d\tEIP: 0x%x\n", recipient->tid, recipient->execState.eip);
    kprintf("EIP: 0x%x\n", *(dword *)(recipient->execState.ebp + 4));
    return E_BLOCK;
  }
  else // no one is waiting to send to this local port, so wait
    attachReceiveQueue(recipient, sender);

  return (int)NULL_TID;
}

port_t *getPort(pid_t pid)
{
  if(pid == NULL_PID)
    return NULL;
  else
  {
    port_t *port=NULL;

    if(treeFind(&portTree, (int)pid, (void **)&port) == E_OK)
      return port;
    else
      return NULL;
  }
}

pid_t getNewPID(void)
{
  int i, maxAttempts=1000;

  lastPID++;

  for(i=1;(treeFind(&portTree, (int)lastPID, NULL) == E_OK && i < maxAttempts)
      || !lastPID;
      lastPID += i*i);

  if(i == maxAttempts)
    return NULL_PID;
  else
    return lastPID;
}

port_t *createPort(void)
{
  return createPortWithPid(getNewPID());
}

port_t *createPortWithPid(pid_t pid)
{
  port_t *port = malloc(sizeof(port_t));

  if(!port)
    return NULL;

  if(treeInsert(&portTree, pid, port) != E_OK)
  {
    free(port);
    return NULL;
  }

  port->pid = pid;
  port->owner = NULL;
  queueInit(&port->senderWaitQueue);

  return port;
}

void releasePort(port_t *port)
{
  if(port)
  {
    treeRemove(&portTree, port->pid, NULL);

    // XXX: notify any waiting threads

    queueDestroy(&port->senderWaitQueue);
    free(port);
  }
}
