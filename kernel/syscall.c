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

void _syscall(TCB *tcb);
static int sysReceive(TCB *tcb, struct PortPair pair, int blocking);
static int sysRpc(TCB *tcb, struct PortPair pair, int blocking);

static int sysExit( TCB *tcb, ... );
static int sysCreate( TCB *tcb, ... );
static int sysRead( TCB *tcb, ... );
static int sysUpdate( TCB *tcb, ... );
static int sysDestroy( TCB *tcb, ... );
static int sysWait( TCB *tcb, ... );

int (*syscallMsgTable[])(TCB *, ...) =
{
  sysExit,
  sysWait,
  sysCreate,
  sysRead,
  sysUpdate,
  sysDestroy
};

int (*syscallTable[])(TCB *, struct PortPair, int) =
{
  sysRpc,
  sysReceive
};

static int sysExit(TCB *tcb, ...)
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

static int sysRpc(TCB *tcb, struct PortPair pair, int blocking)
{
  ExecutionState *execState = &tcb->execState;
  unsigned int call = execState->user.eax & 0xFFFF;

  if(pair.remote == NULL_TID)
  {
    if(call == IRQ_MSG)
    {
      int irq = execState->user.ecx;

      if(irq >= 0 && irq < NUM_IRQS && pair.local == IRQHandlers[irq]
          && getPort(pair.local)->owner == GET_TID(tcb))
      {
        enableIRQ(irq);
        return 0;
      }
    }
    else if(call > sizeof syscallMsgTable / sizeof syscallMsgTable)
      return syscallMsgTable[call](tcb, execState->user.ecx, execState->user.edx);
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

static int sysWait(TCB *tcb, ...)
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

int sysReceive(TCB *tcb, struct PortPair pair, int blocking)
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

static int sysCreate(TCB *tcb, ...)
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

      if(likely(writePmapEntry(args->pbase,args->entry,args->buffer) == 0))
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

      TCB *newTcb = createThread(args->address, args->addr_space,
                   args->stack, args->ex_handler);

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
      struct Port *port;

      if(args->port >= MAX_PORTS)
        return ESYS_ARG;

      if(args->port == NULL_PID)
      {
        static pid_t lastPid = MAX_PORTS / 2;
        pid_t pid = lastPid;

        do
        {
          port = getPort(pid);

          if(port->owner == NULL_PID)
          {
            port->owner = GET_TID(tcb);
            port->sendWaitTail = NULL_PID;

            return pid;
          }
          pid = (pid == MAX_PORTS ? MAX_PORTS / 2 : pid + 1);
        } while(pid != lastPid);

        return ESYS_FAIL;
      }
      else
      {
      	port = getPort(args->port);

      	if(port->owner != NULL_TID)
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

static int sysRead(TCB *tcb, ...)
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

      if(likely(readPmapEntry(args->pbase, args->entry, args->buffer) == 0))
        return ESYS_OK;
      else
        return ESYS_FAIL;
    }
    case RES_TCB:
    {
      struct SyscallReadTcbArgs *args = (struct SyscallReadTcbArgs *)argBuffer;

      TCB *tcb = getTcb(args->tid);

      args->tcb.cr3 = tcb->cr3;
      args->tcb.status = tcb->threadState;
      args->tcb.exHandler = tcb->exHandler;
      args->tcb.priority = tcb->priority;
      args->tcb.state.eax = tcb->execState.eax;
      args->tcb.state.ebx = tcb->execState.ebx;
      args->tcb.state.ecx = tcb->execState.ecx;
      args->tcb.state.edx = tcb->execState.edx;
      args->tcb.state.esi = tcb->execState.esi;
      args->tcb.state.edi = tcb->execState.edi;
      args->tcb.state.ebp = tcb->execState.ebp;
      args->tcb.state.esp = tcb->execState.esp;
      args->tcb.state.eip = tcb->execState.eip;
      args->tcb.state.eflags = tcb->execState.eflags;
      args->tcb.waitPort = tcb->waitPort;

      return ESYS_OK;
    }
    default:
      return ESYS_NOTIMPL;
  }

  return ESYS_FAIL;
}

static int sysUpdate(TCB *tcb, ...)
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

      if(likely(writePmapEntry(args->pbase, args->entry, args->buffer) == 0))
        return ESYS_OK;
      else
        return ESYS_FAIL;
    }
    case RES_TCB:
    {
      struct SyscallUpdateTcbArgs *args = (struct SyscallUpdateTcbArgs *)argBuffer;
      TCB *tcb = getTcb(args->tid);

      switch(args->tcb.status)
      {
        case RUNNING:
        case SLEEPING:
        case WAIT_FOR_SEND:
        case WAIT_FOR_RECV:
          return ESYS_FAIL;
      }

//    Don't update cr3 via sys_update
//      args->tcb.cr3 = tcb->cr3;

      if(tcb->waitPort != args->tcb.waitPort && tcb->threadState == WAIT_FOR_RECV)
      {
        detachSendQueue(tcb);
        tcb->waitPort = args->tcb.waitPort;
      }

      tcb->exHandler = args->tcb.exHandler;

      if(tcb->threadState != args->tcb.threadState)
      {
        switch(tcb->threadState)
        {
          case READY:
            detachRunQueue(tcb);
            break;
          case WAIT_FOR_SEND:
            detachReceiveQueue(tcb);
            break;
          case WAIT_FOR_RECV:
            detachSendQueue(tcb);
            break;
          case SLEEPING:
            timerDetach(tcb);
            break;
          default:
            break;
        }

        tcb->priority = args->tcb.priority;

        switch(args->tcb.priority)
        {
          case DEAD:
            releaseThread(tcb);
            return ESYS_OK;
          case ZOMBIE:
            tcb->threadState = ZOMBIE;
            break;
          case READY:
            startThread(tcb);
            break;
          case PAUSED:
            pauseThread(tcb);
            break;
          default:
            return ESYS_FAIL;
        }
      }
      else
      {
        if(tcb->threadState == READY)
        {
          detachRunQueue(tcb);
          tcb->priority = args->tcb.priority;
          attachRunQueue(tcb);
        }
        else
          tcb->priority = args->tcb.priority;
      }

      tcb->execState.eax = args->tcb.state.eax;
      tcb->execState.ebx = args->tcb.state.ebx;
      tcb->execState.ecx = args->tcb.state.ecx;
      tcb->execState.edx = args->tcb.state.edx;
      tcb->execState.esi = args->tcb.state.esi;
      tcb->execState.edi = args->tcb.state.edi;
      tcb->execState.ebp = args->tcb.state.ebp;
      tcb->execState.esp = args->tcb.state.esp;
      tcb->execState.eip = args->tcb.state.eip;
      tcb->execState.eflags = args->tcb.state.eflags;

      return ESYS_OK;
    }
    default:
      return ESYS_NOTIMPL;
  }

  return ESYS_NOTIMPL;
}

static int sysDestroy(TCB *tcb, ...)
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

      if(likely(writePmapEntry(args->pbase,args->entry,&entry) == 0))
        return ESYS_OK;
      else
        return ESYS_FAIL;
    }
    case RES_IHANDLER:
    {
      struct SyscallDestroyIHandlerArgs *args = (struct SyscallDestroyIHandlerArgs *)argBuffer;

      if(args->intNum < 0 || args->intNum >= NUM_IRQS || getPort(IRQHandlers[args->intNum])->owner != GET_TID(tcb))
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

      struct Port *port = getPort(args->port);

      if(port->owner != NULL_TID)
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

void _syscall( TCB *tcb )
{
  ExecutionState *execState = (ExecutionState *)&tcb->execState;

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

  unsigned int call = (execState->user.eax & 0xFF) >> 1;
  struct PortPair *pair = (struct PortPair *)&execState->user.ebx;

  if(call > sizeof syscallTable / sizeof syscallTable)
  {
    execState->user.eax = (execState->user.eax & 0xFFFF0000)
      | (word)syscallTable[call](tcb, *pair,
                                   execState->user.eax & 1);
  }
  else
  {
    kprintf("Error: Invalid system call 0x%x by TID: %d at EIP: 0x%x\n",
      execState->user.eax & 0xFF, GET_TID(tcb), tcb->execState.user.eip);
    execState->user.eax = (execState->user.eax & 0xFFFF0000)
      | (word)ESYS_BADCALL;
  }
}
