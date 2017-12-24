#include <kernel/memory.h>
#include <kernel/mm.h>
#include <kernel/debug.h>
#include <kernel/syscall.h>
#include <kernel/schedule.h>
#include <kernel/error.h>
#include <kernel/message.h>
#include <os/message.h>
#include <kernel/thread.h>
#include <kernel/interrupt.h>

void _syscall( TCB *tcb );

static int sysSend( TCB *tcb );
static int sysWait( TCB *tcb );
static int sysExit( TCB *tcb );
static int sysCreate( TCB *tcb );
static int sysRead( TCB *tcb );
static int sysUpdate( TCB *tcb );
static int sysDestroy( TCB *tcb );

int (*syscallTable[])(TCB *) =
{
  sysExit,
  sysSend,
  sysWait,
  sysCreate,
  sysRead,
  sysUpdate,
  sysDestroy
};

static int sysExit( TCB *tcb )
{
  if( unlikely(tcb->exHandler == NULL_TID) )
  {
    kprintf("TID: %d has no handler. Releasing...\n", GET_TID(tcb));
    releaseThread(tcb);
  }
  else
    tcb->threadState = ZOMBIE;

  return ESYS_OK;
}

static int sysSend(TCB *tcb)
{
  int retval = sendMessage(tcb, (tid_t)tcb->execState.user.ebx, (struct Message *)tcb->execState.user.ecx, (int)tcb->execState.user.edx);

  switch(retval)
  {
    case 0:
      return ESYS_OK;
    case -2:
      return ESYS_ARG;
    case -3:
      return ESYS_NOTREADY;
    case -1:
    default:
      break;
  }

  return ESYS_FAIL;
}

static int sysWait(TCB *tcb)
{
  if(unlikely((tid_t)tcb->execState.user.ebx == NULL_TID))
  {
    int timeout = (int)tcb->execState.user.edx;

    if(timeout < 0)
    {
      if(likely(pauseThread(tcb)) == 0)
        return ESYS_OK;
      else
        return ESYS_FAIL;
    }
    else if(timeout > 0)
    {
      switch(sleepThread(tcb, timeout))
      {
        case 0:
          return ESYS_OK;
        case -1:
          return ESYS_ARG;
        case 1:
        case -2:
        default:
          return ESYS_FAIL;
      }
    }
    else
    {
      tcb->reschedule = 1;
      return ESYS_OK;
    }
  }
  else
  {
    int retval = receiveMessage(tcb, (tid_t)tcb->execState.user.ebx, (struct Message *)tcb->execState.user.ecx,
                   (int)tcb->execState.user.edx);

    switch(retval)
    {
      case 0:
        return ESYS_OK;
      case -2:
        return ESYS_ARG;
      case -3:
        return ESYS_NOTREADY;
      case -1:
      default:
        return ESYS_FAIL;
    }
  }

  return ESYS_FAIL;
}

static int sysCreate(TCB *tcb)
{
  if(!tcb->privileged)
    return ESYS_PERM;

  switch(tcb->execState.user.ebx)
  {
    case RES_MAPPING:
    case RES_IHANDLER:
    {
      if(likely(registerInt(getTcb((tid_t)tcb->execState.user.ecx), (int)tcb->execState.user.edx) == 0))
        return ESYS_OK;
      else
        return ESYS_FAIL;
    }
    case RES_TCB:
    {
      struct SyscallCreateTcbArgs *args = (struct SyscallCreateTcbArgs *)tcb->execState.user.ecx;

      if(unlikely(args == NULL))
        return ESYS_ARG;

      TCB *newTcb = createThread(args->address, args->addr_space,
                   args->stack, getTcb(args->ex_handler));

      if(unlikely(newTcb == NULL))
        return ESYS_FAIL;
      else if(unlikely(enqueue(&runQueues[newTcb->priority], newTcb) == NULL))
      {
        releaseThread(newTcb);
        return ESYS_FAIL;
      }
      else
        return ESYS_OK;
    }
    default:
      return ESYS_NOTIMPL;
  }

  return ESYS_FAIL;
}

static int sysRead(TCB *tcb)
{
  if(!tcb->privileged)
    return ESYS_PERM;

  return ESYS_NOTIMPL;
}

static int sysUpdate(TCB *tcb)
{
  if(!tcb->privileged)
    return ESYS_PERM;

  return ESYS_NOTIMPL;
}

static int sysDestroy(TCB *tcb)
{
  if(!tcb->privileged)
    return ESYS_PERM;

  return ESYS_NOTIMPL;
}

// Handles a system call

void _syscall( TCB *tcb )
{
  ExecutionState *execState = (ExecutionState *)&tcb->execState;
  int *result = (int *)&execState->user.eax;

  /* sent as a message to kernel?
    can only batch create, read, update, destroy
    should put batch limits to prevent DoS attacks
    Batching of syscalls:
       int count
       struct syscall {
         int call_name;
         int args[];
       } syscalls;

    int returnValues[]
  */

  if(execState->user.eax > sizeof syscallTable / sizeof syscallTable)
    *result = syscallTable[execState->user.eax](tcb);
  else
  {
    kprintf("Error: Invalid system call 0x%x by TID: %d at EIP: 0x%x\n",
      execState->user.eax, GET_TID(tcb), tcb->execState.user.eip);
    *result = ESYS_BADCALL;
  }
}
