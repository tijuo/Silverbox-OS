#include <kernel/mm.h>
#include <kernel/debug.h>
#include <kernel/thread.h>
#include <kernel/schedule.h>
#include <kernel/syscall.h>
#include <kernel/memory.h>
#include <os/message.h>
#include <os/signal.h>

#define NULL_MSG_INDEX -1

int sendMessage( TCB *tcb, tid_t recipient, struct Message *msg, int timeout );
int receiveMessage( TCB *tcb, tid_t sender, struct Message *buf, int timeout );

/**
  Synchronously sends a message from the current thread to the recipient thread.

  If the recipient is not ready to receive the message, then wait.

  @note sendMessage() modifies msg->sender.

  @param tcb The TCB of the sender.
  @param recipient The TID of the message's recipient.
  @param msg The message to be sent.
  @param timeout Aborts the operation after timeout, if non-negative.
  @return 0 on success. -1 on failure. -2 if interrupted by a signal.
*/

int sendMessage( TCB *tcb, tid_t recipient, struct Message *msg, int timeout )
{
  assert( tcb != NULL );
  assert( tcb->state == RUNNING );
  assert( recipient != NULL_TID );
  assert( GET_TID(tcb) != NULL_TID );
  assert( msg != NULL );

  if( recipient == NULL_TID || msg == NULL )
  {
    kprintf("NULL recipient of message\n");
    return -1;
  }

  msg->sender = GET_TID(tcb);

  /* This may cause problems if the receiver is receiving into a NULL
     or non-mapped buffer */

  if( tcbTable[recipient].state == WAIT_FOR_SEND && 
      (tcbTable[recipient].wait_tid == NULL_TID || 
      tcbTable[recipient].wait_tid == GET_TID(tcb)) )
  {
    kprintf("%d is sending to %d\n", GET_TID(tcb), recipient);

    /* edx is the register for the receiver's buffer */
    if( pokeMem((void *)tcbTable[recipient].regs.ecx, MSG_LEN, msg, tcbTable[recipient].addrSpace) != 0 )
    {
      kprintf("Failed to poke\n");
      return -1;
    }
    else
    {
      tcbTable[recipient].state = READY;
      attachRunQueue( &tcbTable[recipient] );
      tcbTable[recipient].wait_tid = NULL_TID;
      tcbTable[recipient].regs.eax = 0;
      return 0;
    }
  }
  else
  {
    if( tcb->state == READY )
      detachRunQueue( tcb );

    tcb->state = WAIT_FOR_RECV;
    tcb->wait_tid = recipient;
    kprintf("%d is waiting to send to %d\n", GET_TID(tcb), recipient);
    enqueue( &tcbTable[recipient].threadQueue, GET_TID(tcb) );
  }
  return -1;
}

/**
  Synchronously receives a message from a thread.

  If no message can be received, then wait for a message from sender.
  
  @param tcb The TCB of the recipient.
  @param sender The TID of the message's sender. May be NULL_TID 
         (which represents anyone).
  @param buf A buffer to hold the incoming message.
  @param timeout Aborts the operation after timeout, if non-negative.
  @return 0 on success. -1 on failure. -2 if interrupted by a signal.
*/

int receiveMessage( TCB *tcb, tid_t sender, struct Message *buf, int timeout )
{
  tid_t send_tid;

  assert( tcb != NULL );
  assert( tcb->state == RUNNING );
  assert( GET_TID(tcb) != NULL_TID );

  if( buf == NULL )
    return -1;

  /* This may cause problems if the receiver is receiving into a NULL
     or non-mapped buffer */

  if( sender == NULL_TID )
    send_tid = dequeue( &tcbTable[GET_TID(tcb)].threadQueue );
  else
    send_tid = detachQueue( &tcbTable[GET_TID(tcb)].threadQueue, sender );

  if( send_tid != NULL_TID )
  {
    kprintf("%d is receiving a message from %d\n", GET_TID(tcb), send_tid);
    /* edx is the register for the receiver's buffer */
    if( peekMem((void *)tcbTable[send_tid].regs.ecx, MSG_LEN, buf, tcbTable[send_tid].addrSpace) != 0 ) 
    {
      enqueue( &tcbTable[GET_TID(tcb)].threadQueue, send_tid );
      kprintf("Failed to peek\n");
      return -1;
    }
    else
    {
      if( tcbTable[send_tid].state == WAIT_FOR_RECV )
      {
        tcbTable[send_tid].state = READY;
        attachRunQueue( &tcbTable[send_tid] );
        tcbTable[send_tid].regs.eax = 0;
      }
      else
        kprintf("Something went wrong\n");

      return 0;
    }
  }
  else
  {
    if( tcb->state == READY )
      detachRunQueue( tcb );

    if( sender == NULL_TID )
      kprintf("%d is waiting for a message from anyone\n", GET_TID(tcb));
    else
      kprintf("%d is waiting for a message from %d\n", GET_TID(tcb), sender);

    tcb->state = WAIT_FOR_SEND;
    tcb->wait_tid = sender;
  }

  return -1;
}
