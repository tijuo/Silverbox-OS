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

void attachSendQueue(tcb_t *tcb, pid_t pid)
{
  port_t *port = getPort(pid);

  if(!port)
    return;

  if( tcb->threadState == READY )
    detachRunQueue( tcb );

  tcb->threadState = WAIT_FOR_RECV;
  tcb->waitPort = pid;

  if(port->sendWaitTail != NULL_TID)
    getTcb(port->sendWaitTail)->queue.next = tcb->tid;

  tcb->queue.next = NULL_TID;
  tcb->queue.prev = port->sendWaitTail;
  port->sendWaitTail = tcb->tid;
}

void attachReceiveQueue(tcb_t *tcb, pid_t pid)
{
  if( tcb->threadState == READY )
    detachRunQueue( tcb );

  tcb->threadState = WAIT_FOR_SEND;
  tcb->waitPort = pid;
}

void detachSendQueue(tcb_t *tcb, pid_t pid)
{
  port_t *port = getPort(pid);

  if(!port)
    return;

  if(tcb->queue.prev != NULL_TID)
    getTcb(tcb->queue.prev)->queue.next = tcb->queue.next;

  if(tcb->queue.next != NULL_TID)
    getTcb(tcb->queue.next)->queue.prev = tcb->queue.prev;

  if(port->sendWaitTail == tcb->tid)
    port->sendWaitTail = tcb->queue.prev;

  tcb->queue.next = tcb->queue.prev = NULL_TID;
  tcb->waitPort = NULL_PID;
  tcb->threadState = READY;
  attachRunQueue(tcb);
}

void detachReceiveQueue(tcb_t *tcb, pid_t pid)
{
  tcb->waitPort = NULL_PID;
  tcb->threadState = READY;

  attachRunQueue(tcb);
}

/**
  Synchronously sends a message from the current thread to the recipient thread.

  If the recipient is not ready to receive the message, then wait unless
  non-blocking.

  @param tcb The TCB of the sender.
  @param portPair The local, remote pair of ports. Upper 16-bits
         is the local port, lower 16-bits is the remote port.
  @param block Non-zero if a blocking send (i.e. wait until a receiver
         receives from the local port). Otherwise, give up if no receivers
         are available.
  @return 0 on success. -1 on failure. -2 on bad argument. -3 if no recipient is ready to receive (and not blocking).
*/

/* XXX: sendMessage() and receiveMessage() won't work for kernel threads. */

int sendMessage(tcb_t *tcb, port_pair_t portPair, int block,
  int args[5])
{
  tcb_t *rec_thread;
  port_t *remote, *local;

  assert( tcb != NULL );
  assert( tcb->threadState == RUNNING );

  if(portPair.remote != NULL_PID)
    remote = getPort(portPair.remote);
  else
    remote = NULL;

  if(portPair.local != NULL_PID)
    local = getPort(portPair.local);
  else
    local = NULL;

  if(remote == NULL
     || (local == NULL && portPair.local != NULL_PID)
     || tcb->tid == remote->owner)
  {
    kprintf("Invalid port.\n");
    return -2;
  }
  else if(local != NULL && local->owner != tcb->tid)
  {
    kprintf("Thread doesn't own local port.\n");
    return -1;
  }

  rec_thread = getTcb(remote->owner);

  /* This may cause problems if the receiver is receiving into a NULL
     or non-mapped buffer */

  // If the recipient is waiting for a message from this sender or any sender

  if(rec_thread->threadState == WAIT_FOR_SEND
     && (rec_thread->waitPort == NULL_PID
         || rec_thread->waitPort == portPair.local))
  {
    if(likely(args == NULL))
    {
      rec_thread->execState.user.eax = tcb->execState.user.eax & 0xFFFF0000;
      rec_thread->execState.user.ebx = (tcb->execState.user.ebx << 16)
                               | (tcb->execState.user.ebx >> 16);
      rec_thread->execState.user.ecx = tcb->execState.user.ecx;
      rec_thread->execState.user.edx = tcb->execState.user.edx;
      rec_thread->execState.user.esi = tcb->execState.user.esi;
      rec_thread->execState.user.edi = tcb->execState.user.edi;
    }
    else
    {
      rec_thread->execState.user.eax = args[0] << 16;
      rec_thread->execState.user.ebx = (portPair.remote << 16) | portPair.local;
      rec_thread->execState.user.ecx = args[1];
      rec_thread->execState.user.edx = args[2];
      rec_thread->execState.user.esi = args[3];
      rec_thread->execState.user.edi = (dword)args[4];
    }

    startThread(rec_thread);
    rec_thread->execState.user.eax |= ESYS_OK;
  }
  else if( !block )	// A timeout of 0 is indicates not to block
  {
    kprintf("send: Non-blocking. TID: %d\tEIP: 0x%x\n", tcb->tid, tcb->execState.user.eip);
    kprintf("EIP: 0x%x\n", *(dword *)(tcb->execState.user.ebp + 4));
    return -3;
  }
  else	// Wait until the recipient is ready to receive the message
    attachSendQueue(tcb, portPair.remote);

  return 0;
}

/**
  Synchronously receives a message from a thread.

  If no message can be received, then wait for a message from sender
  unless indicated as non-blocking.

  @param tcb The TCB of the recipient.
  @param portPair The local, remote pair of ports. Upper 16-bits
         is the local port, lower 16-bits is the remote port. A
         NULL_PID for remote port indicates to receive from anyone.
  @param block Non-zero if a blocking receive (i.e. wait until a sender
         sends to the local port). Otherwise, give up if no senders
         are available.
  @return 0 on success. -1 on failure. -2 on bad argument. -3 if no messages are pending to be received (and non-blocking).
*/

int receiveMessage( tcb_t *tcb, port_pair_t portPair, int block )
{
  tcb_t *send_thread;
  port_t *local, *remote;

  assert( tcb != NULL );
  assert( tcb->threadState == RUNNING );

  if(portPair.local != NULL_PID)
    local = getPort(portPair.local);
  else
    local = NULL;

  if(portPair.remote != NULL_PID)
    remote = getPort(portPair.remote);
  else
    remote = NULL;

  if(local == NULL
     || (remote == NULL && portPair.remote != NULL_PID)
     || (remote != NULL && tcb->tid == remote->owner))
  {
    kprintf("Invalid port.\n");
    return -2;
  }
  else if(local->owner != tcb->tid)
  {
    kprintf("Thread doesn't own local port.\n");
    return -1;
  }

  if( remote == NULL ) // receive message from anyone
    send_thread = getTcb(local->sendWaitTail);
  else // receive message from particular port
  {
    send_thread = getTcb(remote->owner);

    if(!(send_thread && send_thread->threadState == WAIT_FOR_RECV &&
       send_thread->waitPort == portPair.local))
    {
      send_thread = NULL;
    }
  }

  if( send_thread != NULL )
  {
    tcb->execState.user.eax = send_thread->execState.user.eax & 0xFFFF0000;
    tcb->execState.user.ecx = send_thread->execState.user.ecx;
    tcb->execState.user.edx = send_thread->execState.user.edx;
    tcb->execState.user.esi = send_thread->execState.user.esi;
    tcb->execState.user.edi = send_thread->execState.user.edi;
    tcb->execState.user.ebx = (portPair.local << 16) | (send_thread->execState.user.ebx >> 16);

    if(send_thread->execState.user.eax == SYS_RPC
       || send_thread->execState.user.eax == SYS_RPC_BLOCK)
    {
      port_pair_t senderPair;

      senderPair.remote = portPair.local;
      senderPair.local = portPair.remote;

      if(receiveMessage(send_thread, senderPair,
         send_thread->execState.user.eax == SYS_RPC_BLOCK) != 0)
      {
        send_thread->execState.user.eax = ESYS_FAIL;
      }
    }
    else
    {
      startThread(send_thread);
      send_thread->execState.user.eax = ESYS_OK;
    }
  }
  else if( !block )
  {
    kprintf("receive: Non-blocking. TID: %d\tEIP: 0x%x\n", tcb->tid, tcb->execState.user.eip);
    kprintf("EIP: 0x%x\n", *(dword *)(tcb->execState.user.ebp + 4));
    return -3;
  }
  else // no one is waiting to send to this local port, so wait
    attachReceiveQueue(tcb, portPair.remote);

  return 0;
}

port_t *getPort(pid_t pid)
{
  if(pid == NULL_PID)
    return NULL;
  else
  {
    port_t *port=NULL;

    if(tree_find(&portTree, (int)pid, (void **)&port) == E_OK)
      return port;
    else
      return NULL;
  }
}

pid_t getNewPID(void)
{
  int i, maxAttempts=1000;

  lastPID++;

  for(i=1;(tree_find(&portTree, (int)lastPID, NULL) == E_OK && i < maxAttempts)
      || !lastPID;
      lastPID += i*i);

  if(i == maxAttempts)
    return NULL_PID;
  else
    return lastPID;
}

port_t *createPort(void)
{
  port_t *port = malloc(sizeof(port_t));

  if(!port)
    return NULL;

  port->pid = getNewPID();
  port->owner = NULL_TID;
  port->sendWaitTail = NULL_TID;


  return port;
}
