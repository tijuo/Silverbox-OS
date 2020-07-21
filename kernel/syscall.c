#include <kernel/memory.h>
#include <kernel/mm.h>
#include <kernel/debug.h>
#include <kernel/schedule.h>
#include <kernel/error.h>
#include <kernel/message.h>
#include <os/message.h>
#include <kernel/thread.h>
#include <kernel/interrupt.h>
#include <kernel/lowlevel.h>
#include <kernel/pic.h>
#include <stdarg.h>

void _syscall(tcb_t *tcb);
static int sysReceive(tcb_t *tcb, port_pair_t pair, int blocking);
static int sysRpc(tcb_t *tcb, port_pair_t pair, int blocking);

static int sysExit( tcb_t *tcb, ... );
static int sysCreate( tcb_t *tcb, ... );
static int sysRead( tcb_t *tcb, ... );
static int sysUpdate( tcb_t *tcb, ... );
static int sysDestroy( tcb_t *tcb, ... );
static int sysWait( tcb_t *tcb, ... );

int (*syscallMsgTable[])(tcb_t *, ...) =
{
  sysExit,
  sysWait,
  sysCreate,
  sysRead,
  sysUpdate,
  sysDestroy
};

int (*syscallTable[])(tcb_t *, port_pair_t, int) =
{
  sysRpc,
  sysReceive
};

static int sysExit(tcb_t *tcb, ...)
{
  if( unlikely(tcb->exHandler == NULL_PID
      || getPort(tcb->exHandler)->owner == NULL_PID) )
  {
    kprintf("TID: %d has no handler. Releasing...\n", GET_TID(tcb));
    releaseThread(tcb);
  }
  else
    tcb->threadState = ZOMBIE;

  return ESYS_OK;
}

static int sysRpc(tcb_t *tcb, port_pair_t pair, int blocking)
{
  unsigned int call = tcb->execState.user.eax & 0xFFFF;

  if(pair.remote == NULL_TID)
  {
    if(call == IRQ_MSG)
    {
      int irq = tcb->execState.user.ecx;

      port_t *local = getPort(pair.local);

      if(!local)
        return ESYS_ARG;
      else if(irq >= 0 && irq < NUM_IRQS && pair.local == IRQHandlers[irq]
          && local->owner == GET_TID(tcb))
      {
        enableIRQ(irq);
        return 0;
      }
    }
    else if(call > sizeof syscallMsgTable / sizeof syscallMsgTable)
      return syscallMsgTable[call](tcb, tcb->execState.user.ecx, tcb->execState.user.edx);
    else
      return ESYS_BADCALL;
  }
  else
  {
    switch(sendMessage(tcb, pair, blocking, NULL))
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
  }

  return ESYS_FAIL;
}

static int sysWait(tcb_t *tcb, ...)
{
  va_list args;
  va_start(args, tcb);

  unsigned int timeout = va_arg(args, unsigned int);

  va_end(args);

  if(timeout < TIMEOUT_INF)
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
        break;
    }
    return ESYS_FAIL;
  }
  else
  {
    if(tcb->threadState == RUNNING)
      tcb->threadState = READY;

    return ESYS_OK;
  }
}

int sysReceive(tcb_t *tcb, port_pair_t pair, int blocking)
{
  switch(receiveMessage(tcb, pair, blocking))
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

static int sysCreate(tcb_t *tcb, ...)
{
  if(!tcb->privileged)
    return ESYS_PERM;

  va_list vaargs;
  va_start(vaargs, tcb);

  int resource = va_arg(vaargs, int);
  void *argBuffer = va_arg(vaargs, void *);

  va_end(vaargs);

  switch(resource)
  {
    case RES_MAPPING:
    {
      struct SyscallCreateMappingArgs *args = (struct SyscallCreateMappingArgs *)argBuffer;

      if(likely(writePmapEntry(args->addrSpace,args->entry,args->buffer) == 0))
        return ESYS_OK;
      else
        return ESYS_FAIL;
    }
    case RES_IHANDLER:
    {
      struct SyscallCreateIHandlerArgs *args = (struct SyscallCreateIHandlerArgs *)argBuffer;

      if(likely(registerInt(args->handler, args->intNum) == 0))
        return ESYS_OK;
      else
        return ESYS_FAIL;
    }
    case RES_TCB:
    {
      struct SyscallCreateTcbArgs *args = (struct SyscallCreateTcbArgs *)argBuffer;

      if(unlikely(args == NULL))
        return ESYS_ARG;

      tcb_t *newTcb = createThread(args->entry, args->addrSpace,
                   args->stack, args->exHandler);

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
    case RES_PORT:
    {
      struct SyscallCreatePortArgs *args = (struct SyscallCreatePortArgs *)argBuffer;
      port_t *port;

      if(args->port == NULL_PID)
      {
        port = createPort();
        return port ? port->pid : ESYS_FAIL;
      }
      else
      {
      	port = getPort(args->port);

      	if(!port)
          return ESYS_FAIL;
      	else
      	{
          port->owner = GET_TID(tcb);
          port->sendWaitTail = NULL_PID;

          return ESYS_OK;
        }
      }
    }
    default:
      return ESYS_NOTIMPL;
  }

  return ESYS_FAIL;
}

static int sysRead(tcb_t *tcb, ...)
{
  if(!tcb->privileged)
    return ESYS_PERM;

  va_list vaargs;
  va_start(vaargs, tcb);

  int resource = va_arg(vaargs, int);
  void *argBuffer = va_arg(vaargs, void *);

  va_end(vaargs);

  switch(resource)
  {
    case RES_MAPPING:
    {
      struct SyscallReadMappingArgs *args = (struct SyscallReadMappingArgs *)argBuffer;

      if(likely(readPmapEntry(args->addrSpace, args->entry, args->buffer) == 0))
        return ESYS_OK;
      else
        return ESYS_FAIL;
    }
    case RES_TCB:
    {
      struct SyscallReadTcbArgs *args = (struct SyscallReadTcbArgs *)argBuffer;

      tcb_t *targetTcb = getTcb(args->tid);

      if(!targetTcb)
        return ESYS_FAIL;

      args->tcb->addrSpace = targetTcb->cr3;
      args->tcb->status = targetTcb->threadState;
      args->tcb->exHandler = targetTcb->exHandler;
      args->tcb->priority = targetTcb->priority;
      args->tcb->state.eax = targetTcb->execState.user.eax;
      args->tcb->state.ebx = targetTcb->execState.user.ebx;
      args->tcb->state.ecx = targetTcb->execState.user.ecx;
      args->tcb->state.edx = targetTcb->execState.user.edx;
      args->tcb->state.esi = targetTcb->execState.user.esi;
      args->tcb->state.edi = targetTcb->execState.user.edi;
      args->tcb->state.ebp = targetTcb->execState.user.ebp;
      args->tcb->state.esp = targetTcb->execState.user.userEsp;
      args->tcb->state.eip = targetTcb->execState.user.eip;
      args->tcb->state.eflags = targetTcb->execState.user.eflags;

      return ESYS_OK;
    }
    default:
      return ESYS_NOTIMPL;
  }

  return ESYS_FAIL;
}

static int sysUpdate(tcb_t *tcb, ...)
{
  if(!tcb->privileged)
    return ESYS_PERM;

  va_list vaargs;
  va_start(vaargs, tcb);

  int resource = va_arg(vaargs, int);
  void *argBuffer = va_arg(vaargs, void *);

  va_end(vaargs);

  switch(resource)
  {
    case RES_MAPPING:
    {
      struct SyscallUpdateMappingArgs *args = (struct SyscallUpdateMappingArgs *)argBuffer;

      if(likely(writePmapEntry(args->addrSpace, args->entry, args->buffer) == 0))
        return ESYS_OK;
      else
        return ESYS_FAIL;
    }
    case RES_TCB:
    {
      struct SyscallUpdateTcbArgs *args = (struct SyscallUpdateTcbArgs *)argBuffer;
      tcb_t *targetTcb = getTcb(args->tid);

      if(!targetTcb)
        return ESYS_FAIL;

      switch(args->tcb->status)
      {
        case RUNNING:
        case SLEEPING:
        case WAIT_FOR_SEND:
        case WAIT_FOR_RECV:
          return ESYS_FAIL;
      }

//    Don't update cr3 via sys_update
//      args->tcb->cr3 = tcb->cr3;

      targetTcb->exHandler = args->tcb->exHandler;

      if(targetTcb->threadState != args->tcb->status)
      {
        switch(targetTcb->threadState)
        {
          case READY:
            detachRunQueue(targetTcb);
            break;
          case WAIT_FOR_SEND:
            detachReceiveQueue(targetTcb, targetTcb->waitPort);
            break;
          case WAIT_FOR_RECV:
            detachSendQueue(targetTcb, targetTcb->waitPort);
            break;
          case SLEEPING:
            timerDetach(targetTcb);
            break;
          default:
            break;
        }

        targetTcb->priority = args->tcb->priority;

        switch(args->tcb->priority)
        {
          case DEAD:
            releaseThread(targetTcb);
            return ESYS_OK;
          case ZOMBIE:
            targetTcb->threadState = ZOMBIE;
            break;
          case READY:
            startThread(targetTcb);
            break;
          case PAUSED:
            pauseThread(targetTcb);
            break;
          default:
            return ESYS_FAIL;
        }
      }
      else
      {
        if(targetTcb->threadState == READY)
        {
          detachRunQueue(targetTcb);
          targetTcb->priority = args->tcb->priority;
          attachRunQueue(targetTcb);
        }
        else
          targetTcb->priority = args->tcb->priority;
      }

      targetTcb->execState.user.eax = args->tcb->state.eax;
      targetTcb->execState.user.ebx = args->tcb->state.ebx;
      targetTcb->execState.user.ecx = args->tcb->state.ecx;
      targetTcb->execState.user.edx = args->tcb->state.edx;
      targetTcb->execState.user.esi = args->tcb->state.esi;
      targetTcb->execState.user.edi = args->tcb->state.edi;
      targetTcb->execState.user.ebp = args->tcb->state.ebp;
      targetTcb->execState.user.userEsp = args->tcb->state.esp;
      targetTcb->execState.user.eip = args->tcb->state.eip;
      targetTcb->execState.user.eflags = args->tcb->state.eflags;

      return ESYS_OK;
    }
    default:
      return ESYS_NOTIMPL;
  }

  return ESYS_NOTIMPL;
}

static int sysDestroy(tcb_t *tcb, ...)
{
  if(!tcb->privileged)
    return ESYS_PERM;

  va_list vaargs;
  va_start(vaargs, tcb);

  int resource = va_arg(vaargs, int);
  void *argBuffer = va_arg(vaargs, void *);

  va_end(vaargs);

  switch(resource)
  {
    case RES_MAPPING:
    {
      int entry = 0;
      struct SyscallDestroyMappingArgs *args = (struct SyscallDestroyMappingArgs *)argBuffer;

      if(likely(writePmapEntry(args->addrSpace,args->entry,&entry) == 0))
        return ESYS_OK;
      else
        return ESYS_FAIL;
    }
    case RES_IHANDLER:
    {
      struct SyscallDestroyIHandlerArgs *args = (struct SyscallDestroyIHandlerArgs *)argBuffer;
      port_t *irqHandler = getPort(IRQHandlers[args->intNum]);

      if(args->intNum < 0 || args->intNum >= NUM_IRQS || !irqHandler || irqHandler->owner != GET_TID(tcb))
        return ESYS_FAIL;
      else
      {
        unregisterInt(args->intNum);
        return ESYS_OK;
      }
    }
    case RES_TCB:
    {
      // release all owned ports
      return ESYS_NOTIMPL;
    }
    case RES_PORT:
    {
      struct SyscallDestroyPortArgs *args = (struct SyscallDestroyPortArgs *)argBuffer;

      port_t *port = getPort(args->port);

      if(!port)
        return ESYS_FAIL;
      else
      {
        port->owner = NULL_TID;
        port->sendWaitTail = NULL_PID;

        return ESYS_OK;
      }
    }
    default:
      return ESYS_NOTIMPL;
  }

  return ESYS_NOTIMPL;
}

// Handles a system call

void _syscall( tcb_t *tcb )
{
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

  unsigned int call = (tcb->execState.user.eax & 0xFF) >> 1;
  port_pair_t *pair = (port_pair_t *)&tcb->execState.user.ebx;

  if(call > sizeof syscallTable / sizeof syscallTable)
  {
    tcb->execState.user.eax = (tcb->execState.user.eax & 0xFFFF0000)
      | (word)syscallTable[call](tcb, *pair,
                                   tcb->execState.user.eax & 1);
  }
  else
  {
    kprintf("Error: Invalid system call 0x%x by TID: %d at EIP: 0x%x\n",
      tcb->execState.user.eax & 0xFF, GET_TID(tcb), tcb->execState.user.eip);
    tcb->execState.user.eax = (tcb->execState.user.eax & 0xFFFF0000)
      | (word)ESYS_BADCALL;
  }
}
