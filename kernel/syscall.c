#include <kernel/memory.h>
#include <kernel/mm.h>
#include <kernel/debug.h>
#include <kernel/syscall.h>
#include <kernel/schedule.h>
#include <kernel/error.h>
#include <kernel/message.h>

void _syscall( TCB *tcb );

static void sysExit( TCB *tcb, int code );
static int sysYield( TCB *tcb );

/// Voluntarily gives up control of the current thread

int sysYield( TCB *tcb )
{
  assert( tcb != NULL );
  assert( GET_TID(tcb) != NULL_TID );

  if( tcb == NULL )
    return ESYS_ARG;

  assert(tcb == currentThread);

  tcb->reschedule = 1;
//  tcb->threadState = READY; // Not sure if this actually works
  return ESYS_OK;
}


static void sysExit( TCB *tcb, int code )
{
  tcb->threadState = ZOMBIE;

  if( tcb->exHandler == NULL_TID )
    releaseThread(tcb);
}

// Handles a system call

void _syscall( TCB *tcb )
{
  ExecutionState *execState = (ExecutionState *)&tcb->execState;
  int *result = (int *)&execState->user.eax;

// send/send_and_wait
// wait/sleep/yield
// acquire
// release
// modify
// read
// grant
// revoke
// exit

  switch ( execState->user.eax )
  {
    case SYS_SEND:
      *result = sendMessage(tcb, (tid_t)execState->user.ebx, (void *)execState->user.ecx, (int)execState->user.edx);
      break;
    case SYS_WAIT:
      *result = receiveMessage(tcb, (tid_t)execState->user.ebx, (void *)execState->user.ecx, (int)execState->user.edx);
      break;
    case SYS_EXIT:
      sysExit(tcb, (int)execState->user.ebx);
      *result = ESYS_FAIL; // sys_exit() should never return
      break;
    case SYS_YIELD:
      *result = sysYield(tcb);
    default:
      kprintf("Invalid system call: 0x%x %d 0x%x\n", execState->user.eax,
		GET_TID(tcb), tcb->execState.user.eip);
      assert(false);
      *result = ESYS_BADCALL;
      break;
  }
}
