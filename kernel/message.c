#include <kernel/mm.h>
#include <kernel/debug.h>
#include <kernel/thread.h>
#include <kernel/schedule.h>
#include <kernel/syscall.h>
#include <kernel/memory.h>
#include <os/message.h>
#include <kernel/pit.h>

#define NULL_MSG_INDEX -1

int sendMessage( TCB *tcb, tid_t recipient, struct Message *msg, unsigned int timeout );
int receiveMessage( TCB *tcb, tid_t sender, struct Message *buf, unsigned int timeout );

/**
  Synchronously sends a message from the current thread to the recipient thread.

  If the recipient is not ready to receive the message, then wait.

  @note sendMessage() modifies msg->sender.

  @param tcb The TCB of the sender.
  @param recipient The TID of the message's recipient.
  @param msg The message to be sent.
  @param timeout Aborts the operation after timeout, if non-negative.
  @return 0 on success. -1 on failure. -2 on bad argument. -3 if no recipient is ready to receive (and timeout is 0).
*/

/* XXX: sendMessage() and receiveMessage() won't work for kernel threads. */

int sendMessage( TCB *tcb, tid_t recipient, struct Message *msg, unsigned int timeout )
{
  TCB *rec_thread;

  assert( tcb != NULL );
  assert( tcb->threadState == RUNNING );

  if( GET_TID(tcb) == recipient )
  {
    kprintf("Sending to self\n");
    return -2;
  }

  if( recipient == NULL_TID )
  {
    kprintf("NULL recipient of message.\n");
    return -2;
  }

  if( msg == NULL )
  {
    kprintf("NULL message.\n");
    return -2;
  }

  msg->sender = GET_TID(tcb);

  rec_thread = &tcbTable[recipient];

  /* This may cause problems if the receiver is receiving into a NULL
     or non-mapped buffer */

  // If the recipient is waiting for a message from this sender or any sender

  if( rec_thread->threadState == WAIT_FOR_SEND && (rec_thread->waitThread == NULL ||
      rec_thread->waitThread == tcb) )
  {
//    kprintf("%d is sending to %d\n", GET_TID(tcb), recipient);

    /* ecx is the register for the receiver's buffer */
    if( pokeVirt((addr_t)rec_thread->execState.user.ecx, MSG_LEN, msg,
        rec_thread->cr3.base << 12) != 0 )
    {
      kprintf("Failed to poke. 0x%x -> 0x%x(%d bytes)\n", msg, rec_thread->execState.user.ecx, MSG_LEN);
      return -1;
    }
    else
    {
      timerDetach( rec_thread );
      rec_thread->threadState = READY;
      attachRunQueue( rec_thread );

//      if( rec_thread->priority > 2 )
//        setPriority( rec_thread, rec_thread->priority - 1 );

      rec_thread->waitThread = NULL;
      rec_thread->execState.user.eax = 0;
      return 0;
    }
  }
  else if( timeout == 0 )	// A timeout of 0 is indicates not to block
  {
    kprintf("send: Timeout == 0. TID: %d\tEIP: 0x%x\n", GET_TID(tcb), tcb->execState.user.eip);
    kprintf("EIP: 0x%x\n", *(dword *)(tcb->execState.user.ebp + 4));
    return -3;
  }
  else	// Wait until the recipient is ready to receive the message
  {
    if( tcb->threadState == READY )
      detachRunQueue( tcb );

    tcb->threadState = WAIT_FOR_RECV;
    tcb->waitThread = rec_thread;
  //  kprintf("%d is waiting to send to %d\n", GET_TID(tcb), recipient);
    enqueue( &rec_thread->threadQueue, tcb );

    if( timeout > 0 )
      timerEnqueue( tcb, (timeout * HZ) / 1000 );
  }

  return -1;
}

/**
  Synchronously receives a message from a thread.

  If no message can be received, then wait for a message from sender.

  @param tcb The TCB of the recipient.
  @param sender The TID from which to receive a message. If
         sender is NULL_TID then receive a message from anyone
         that sends.
  @param buf A buffer to hold the incoming message.
  @param timeout Aborts the operation after timeout, if non-negative.
  @return 0 on success. -1 on failure. -2 on bad argument. -3 if no messages are pending to be received (and timeout is 0).
*/

int receiveMessage( TCB *tcb, tid_t sender, struct Message *buf, unsigned int timeout )
{
  TCB *send_thread;

  assert( tcb != NULL );
  assert( tcb->threadState == RUNNING );
  assert( GET_TID(tcb) != NULL_TID );

  if( GET_TID(tcb) == sender )
  {
    kprintf("Receiving from self\n");
    return -2;
  }

  if( buf == NULL )
    return -2;

  /* This may cause problems if the receiver is receiving into a NULL
     or non-mapped buffer */

  if( sender == NULL_TID )
    send_thread = popQueue( &tcb->threadQueue );
  else
    send_thread = detachQueue( &tcb->threadQueue, &tcbTable[sender] );

  if( send_thread != NULL )
  {
  //  kprintf("%d is receiving a message from %d\n", GET_TID(tcb), send_tid);
    /* ecx is the register for the receiver's buffer */
    if( peekVirt((addr_t)send_thread->execState.user.ecx,
        MSG_LEN, buf, send_thread->cr3.base << 12) != 0 )
    {
      enqueue( &tcb->threadQueue, send_thread );
      kprintf("Failed to peek\n");
      return -1;
    }
    else
    {
      if( send_thread->threadState == WAIT_FOR_RECV )
      {
        timerDetach( send_thread );
        send_thread->threadState = READY;
        attachRunQueue( send_thread );
        send_thread->quantaLeft = send_thread->priority + 1;
        send_thread->execState.user.eax = 0;
      }
      else
      {
        kprintf("Something went wrong\n");
      }

      return 0;
    }
  }
  else if( timeout == 0 )
  {
    kprintf("receive: Timeout == 0. TID: %d\tEIP: 0x%x\n", GET_TID(tcb), tcb->execState.user.eip);
    kprintf("EIP: 0x%x\n", *(dword *)(tcb->execState.user.ebp + 4));
    return -3;
  }
  else
  {
    if( tcb->threadState == READY )
      detachRunQueue( tcb );

    tcb->threadState = WAIT_FOR_SEND;
    tcb->waitThread = &tcbTable[sender];

    if( timeout > 0 )
      timerEnqueue( tcb, (timeout * HZ) / 1000 );
  }

  return -1;
}
