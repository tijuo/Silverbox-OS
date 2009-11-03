#include <kernel/signal.h>
#include <string.h>
#include <kernel/queue.h>
#include <kernel/thread.h>
#include <kernel/debug.h>

int sysSetSigHandler( TCB *tcb, void *handler )
{
  tcb->sig_handler = handler;
  return 0;
}

/* Raising a signal when a thread does a __send() or __receive()
   will result in a return code indicated that it had
   been interrupted by a signal. Why?

   Because if a thread is either waiting for a send or a receive it blocks,
   but when a signal is raised, code execution jumps to the signal
   handler, it completes, then the thread resumes execution rather
   than going back into the waiting state.
*/

int sysRaise( TCB *tcb, int signal, int arg )
{
  dword stack[12];
  tid_t t1, t2;

  if( tcb->sig_handler == NULL )
    return 1;

  if( tcb->state == WAIT_FOR_SEND )
  {
    tcb->wait_tid = NULL_TID;
    tcb->state = PAUSED;
    tcb->regs.eax = 2;
    kprintf("receive interrupted\n");
  }
  else if( tcb->state == WAIT_FOR_RECV )
  {
    tcb->regs.eax = 2;

    kprintf("send interrupted\n");
    tcb->state = PAUSED;

    detachQueue( &tcbTable[tcb->wait_tid].threadQueue, GET_TID(tcb) );
    tcb->wait_tid = NULL_TID;
  }

  stack[0] = signal;
  stack[1] = arg;
  memcpy( (void *)&stack[2], (void *)&tcb->regs.edi, 8 * sizeof(dword) );
  stack[10] = tcb->regs.eflags;
  stack[11] = tcb->regs.eip;

  if( pokeMem( (void *)(tcb->regs.userEsp - sizeof stack), sizeof stack, 
           stack, tcb->addrSpace ) != 0 )
  {
    tcb->regs.eax = -1;
    return -1;
  }

  tcb->regs.eip = (dword)tcb->sig_handler;
  tcb->regs.userEsp -= sizeof stack;

  assert( startThread( tcb ) >= 0 );

  kprintf("eax: 0x%x 0x%x\n", tcb->regs.eax, tcbTable[GET_TID(tcb)].regs.eax);

  if( tcb == currentThread )
    return -2;
  else
    return 0;
}
