#include <string.h>
#include <kernel/queue.h>
#include <kernel/thread.h>
#include <kernel/debug.h>
#include <os/signal.h>
#include <os/syscalls.h>

int sysSetSigHandler( TCB *tcb, void *handler );
int sysRaise( TCB *tcb, int signal, int arg );


int sysSetSigHandler( TCB *tcb, void *handler )
{
  tcb->sig_handler = handler;
  return ESYS_OK;
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

  if( tcb->sig_handler == NULL )
    return 1;

  if( tcb == currentThread )
    return -2;

  if( tcb->threadState == WAIT_FOR_SEND || tcb->threadState == WAIT_FOR_RECV )
  {
    if( timerDetach( tcb ) == NULL )
      return ESYS_FAIL;
  }

  if( tcb->threadState == WAIT_FOR_SEND )
  {
    tcb->waitThread = NULL;
    tcb->threadState = PAUSED;
    tcb->execState.user.eax = (dword)(signal == SIGTMOUT ? -3 : -2);
    kprintf("receive interrupted\n");
  }
  else if( tcb->threadState == WAIT_FOR_RECV )
  {
    tcb->execState.user.eax = (dword)(signal == SIGTMOUT ? -3 : -2);

    kprintf("send interrupted\n");
    tcb->threadState = PAUSED;

    detachQueue( &tcb->waitThread->threadQueue, tcb );
    tcb->waitThread = NULL;
  }

  stack[0] = (dword)signal;
  stack[1] = (dword)arg;
  memcpy( &stack[2], &tcb->execState.user.edi, 8 * sizeof(dword) );
  stack[5] = tcb->execState.user.userEsp - sizeof stack;
  stack[10] = tcb->execState.user.eflags;
  stack[11] = tcb->execState.user.eip;

  if( pokeVirt( (addr_t)(tcb->execState.user.userEsp - sizeof stack), sizeof stack,
           (addr_t)stack, (addr_t)(tcb->cr3.base << 12) ) != 0 )
  {
    tcb->execState.user.eax = (dword)-1;
    return ESYS_FAIL;
  }

  tcb->execState.user.eip = (dword)tcb->sig_handler;
  tcb->execState.user.userEsp -= sizeof stack;

  if( tcb->threadState != RUNNING && tcb->threadState != READY )
    startThread( tcb );

  return ESYS_OK;
}
