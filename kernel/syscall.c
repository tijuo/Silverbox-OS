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
#include <kernel/interrupt.h>
#include <oslib.h>
#include <os/syscalls.h>

void _syscall(ExecutionState *state);
static int sysReceive(ExecutionState *state);
static int sysSend(ExecutionState *state);

// Privileged call
static int sysMap(ExecutionState *state);
static int sysUnmap(ExecutionState *state);

static int sysCreateThread(ExecutionState *state);
static int sysDestroyThread(ExecutionState *state);
static int sysReadThread(ExecutionState *state);
static int sysUpdateThread(ExecutionState *state);

static int sysCreatePort(ExecutionState *state);
static int sysDestroyPort(ExecutionState *state);

// End of IRQ

static int sysEoi(ExecutionState *state);

static int sysIrqWait(ExecutionState *state);

// Privileged call
static int sysBindIrq(ExecutionState *state);
// Privileged call
static int sysUnbindIrq(ExecutionState *state);

static int sysExit(ExecutionState *state);
static int sysWait(ExecutionState *state);

// XXX: Need to implement flags

#define PM_READ_ONLY            0
#define PM_READ_WRITE           0x02
#define PM_NOT_CACHED           0x10
#define PM_WRITE_THROUGH        0x08
#define PM_NOT_ACCESSED         0
#define PM_ACCESSED             0x20
#define PM_NOT_DIRTY            0
#define PM_DIRTY                0x40
#define PM_LARGE_PAGE           0x80
#define PM_INVALIDATE           0x80000000

int (*syscallTable[])(ExecutionState *) =
{
  sysExit,
  sysWait,
  sysSend,
  sysReceive,
  sysMap,
  sysUnmap,
  sysCreateThread,
  sysDestroyThread,
  sysReadThread,
  sysUpdateThread,
  sysCreatePort,
  sysDestroyPort,
  sysEoi,
  sysIrqWait,
  sysBindIrq,
  sysUnbindIrq
};

static int sysMap(ExecutionState *state)
{
  asid_t asid = (asid_t)state->ebx;
  addr_t vAddr = (addr_t)state->ecx;
  pframe_t pframe = (pframe_t)state->edx;
  int flags = (int)state->esi;

  if(currentThread->tid == INIT_PAGER_TID)
  {
    paddr_t *addrSpace;
    pde_t pde;
    pte_t pte;

    addrSpace = getAddressSpace(asid);

    if(addrSpace == (paddr_t *)NULL_PADDR || readPmapEntry(*addrSpace, PDE_INDEX(vAddr), &pde) != E_OK)
      return ESYS_FAIL;

    if(!pde.present)
    {
      paddr_t addr;

      if(flags & PM_LARGE_PAGE)
      {
        *(u32 *)&pde = 0;

        pde.base = (u32)pframe;
        pde.rwPriv = (flags & PM_READ_WRITE) == PM_READ_WRITE;
        pde.pwt = (flags & PM_WRITE_THROUGH) == PM_WRITE_THROUGH;
        pde.pcd = (flags & PM_NOT_CACHED) == PM_NOT_CACHED;
        pde.pageSize = pde.usPriv = pde.present = 1;
      }
      else
      {
        addr = allocPageFrame();

        *(u32 *)&pde = 0;
        pde.base = (u32)(addr >> 12);
        pde.rwPriv = pde.usPriv = pde.present = 1;

        clearPhysPage(addr);
      }

      if(writePmapEntry(*addrSpace, PDE_INDEX(vAddr), &pde) != E_OK)
      {
        if(!(flags & PM_LARGE_PAGE))
          freePageFrame(addr);
        return ESYS_FAIL;
      }
      else if(flags & PM_LARGE_PAGE)
        return ESYS_OK;
    }

    if(readPmapEntry((paddr_t)(pde.base << 12), PTE_INDEX(vAddr), &pte) != E_OK)
      return ESYS_FAIL;

    if(pte.present)
      return ESYS_FAIL;
    else
    {
      paddr_t addr = allocPageFrame();

      pte.base = (u32)(addr >> 12);
      pte.rwPriv = (flags & PM_READ_WRITE) == PM_READ_WRITE;
      pte.usPriv = pte.present = 1;
      pde.pwt = (flags & PM_WRITE_THROUGH) == PM_WRITE_THROUGH;
      pde.pcd = (flags & PM_NOT_CACHED) == PM_NOT_CACHED;

      if(writePmapEntry((paddr_t)(pde.base << 12), PTE_INDEX(vAddr), &pte) != E_OK)
      {
        freePageFrame(addr);
        return ESYS_FAIL;
      }
      else
        return ESYS_OK;
    }
  }
  else
    return ESYS_PERM;
}

static int sysUnmap(ExecutionState *state)
{
  asid_t asid = (asid_t)state->ebx;
  addr_t addr = (addr_t)state->ecx;

  if(currentThread->tid == INIT_PAGER_TID)
  {
    pde_t pde;
    pte_t pte;
    paddr_t *addrSpace = getAddressSpace(asid);

    if(addrSpace == (paddr_t *)NULL_PADDR || readPmapEntry(*addrSpace, PDE_INDEX(addr), &pde) != E_OK)
      return ESYS_FAIL;
    else if(!pde.present)
      return ESYS_FAIL;

    if(pde.pageSize)
    {
      pde.present = 0;
      return writePmapEntry(*addrSpace, PDE_INDEX(addr), &pde) == E_OK ? ESYS_OK : ESYS_FAIL;
    }
    else
    {
      if(readPmapEntry((paddr_t)(pde.base << 12), PTE_INDEX(addr), &pte) != E_OK || !pte.present)
        return ESYS_FAIL;

      pte.present = 0;

      return writePmapEntry((paddr_t)(pde.base << 12), PTE_INDEX(addr), &pte) == E_OK ? ESYS_OK : ESYS_FAIL;
    }
  }
  else
    return ESYS_PERM;
}

static int sysCreateThread(ExecutionState *state)
{
  addr_t entry = (addr_t)state->ebx;
  asid_t asid = (asid_t)state->ecx;
  addr_t stackTop = (addr_t)state->edx;
  pid_t exHandler = (pid_t)state->esi;

  if(currentThread->tid == INIT_SERVER_TID)
  {
    paddr_t *paddr = (asid == NULL_ASID) ? NULL : getAddressSpace(asid);

    tcb_t *newTcb = createThread(entry, paddr ? *paddr : NULL_PADDR, stackTop, exHandler);
    return newTcb ? (int)newTcb->tid : ESYS_FAIL;
  }
  else
    return ESYS_PERM;
}

static int sysDestroyThread(ExecutionState *state)
{
  if(currentThread->tid == INIT_SERVER_TID)
  {
    tcb_t *tcb = getTcb((tid_t)state->ebx);
    return tcb && releaseThread(tcb) == E_OK ? ESYS_OK : ESYS_FAIL;
  }
  else
    return ESYS_PERM;
}

static int sysReadThread(ExecutionState *state)
{
  tid_t tid = (tid_t)state->ebx;
  thread_info_t *info = (thread_info_t *)state->ecx;

  return ESYS_NOTIMPL;
}

static int sysUpdateThread(ExecutionState *state)
{
  tid_t tid = (tid_t)state->ebx;
  thread_info_t *info = (thread_info_t *)state->ecx;

  return ESYS_NOTIMPL;
}

static int sysExit(ExecutionState *state)
{
  int status = (int)state->ebx;

  // XXX: send exit status to initial server

  if(currentThread->exHandler == NULL_PID)
    releaseThread(currentThread);
  else
  {
    // XXX: send exit status to exception handler

    currentThread->threadState = ZOMBIE;
  }

  return ESYS_OK;
}

static int sysBindIrq(ExecutionState *state)
{
  tcb_t *tcb = getTcb((tid_t)state->ebx);
  int irq = (int)state->ecx;

  if(currentThread->tid == INIT_SERVER_TID)
    return registerIrq(tcb, irq) == E_OK ? ESYS_OK : ESYS_FAIL;
  else
    return ESYS_PERM;
}

static int sysUnbindIrq(ExecutionState *state)
{
  if(currentThread->tid == INIT_SERVER_TID)
    return unregisterIrq((int)state->ebx) == E_OK ? ESYS_OK : ESYS_FAIL;
  else
    return ESYS_PERM;
}

static int sysEoi(ExecutionState *state)
{
  int irqNum = (int)state->ebx;

  if(!isValidIRQ(irqNum))
    return ESYS_ARG;
  else if(irqHandlers[irqNum] != currentThread)
    return ESYS_PERM;
  else
  {
    enableIRQ(irqNum);
    return ESYS_OK;
  }

  return ESYS_FAIL;
}

/**
  Wait for an IRQ to be raised. The current thread will be notified
  upon such an event.

  @param irqNum - The IRQ to wait for. IRQ_ANY if waiting for any irq
         handled by the current thread.
  @param poll - True, if this call should not block. False, if this call
         should block until receipt of an irq.
  @return irqNum on success. ESYS_FAIL upon failure. ESYS_PERM if the
          current thread doesn't have permisson to receive the irq.
          ESYS_NOTREADY if no irq is pending and the call would block.
          ESYS_ARG upon invalid irqNum.
*/

static int sysIrqWait(ExecutionState *state)
{
  int irqNum = (int)state->ebx;
  int poll = (int)state->ecx;

  if(irqNum == IRQ_ANY)
  {
    bool isHandler = false;

    for(irqNum=0; irqNum < NUM_IRQS; irqNum++)
    {
      if(irqHandlers[irqNum] == currentThread)
      {
        isHandler = true;

        if(pendingIrqBitmap[irqNum / 32] & (1 << (irqNum & 0x1F)))
        {
          pendingIrqBitmap[irqNum / 32] &= ~(1 << (irqNum & 0x1F));
          return irqNum;
        }
      }
    }

    if(!isHandler)
      return ESYS_PERM;
  }
  else if(!isValidIRQ(irqNum))
    return ESYS_ARG;
  else if(irqHandlers[irqNum] != currentThread)
    return ESYS_PERM;
  else if(pendingIrqBitmap[irqNum / 32] & (1 << (irqNum & 0x1F)))
  {
    pendingIrqBitmap[irqNum / 32] &= ~(1 << (irqNum & 0x1F));
    return irqNum;
  }
  else if(poll)
    return ESYS_NOTREADY;

  if(currentThread->threadState == RUNNING
     || (currentThread->threadState == READY && detachRunQueue(currentThread)))
  {
    currentThread->threadState = IRQ_WAIT;
    return ESYS_OK; /* Doesn't actually return this to the user. The return
                       value will be set
                       by the kernel's irq handler stub. */
  }

  return ESYS_FAIL;
}

static int sysCreatePort(ExecutionState *state)
{
  pid_t requestedPid = (pid_t)state->ebx;
  pid_t *outPid = (pid_t *)state->ecx;

  port_t *port;

  if(requestedPid == NULL_PID)
  {
    if(outPid)
      port = createPort();
    else
      return ESYS_ARG;
  }
  else if(getPort(requestedPid) == NULL)
    port = createPortWithPid(requestedPid);
  else
    return ESYS_FAIL;

  if(port)
  {
    port->owner = currentThread;

    if(outPid)
      *outPid = port->pid;

    return ESYS_OK;
  }

  return ESYS_FAIL;
}

static int sysDestroyPort(ExecutionState *state)
{
  port_t *port = getPort((pid_t)state->ebx);

  if(port)
  {
    if(port->owner != currentThread)
      return ESYS_PERM;
    else
    {
      releasePort(port);
      return ESYS_OK;
    }
  }

  return ESYS_FAIL;
}


static int sysSend(ExecutionState *state)
{
  pid_t remotePort = (pid_t)state->ebx;
  int blocking = (int)state->ecx;

  switch(sendMessage(currentThread, remotePort, blocking))
  {
    case E_OK:
      return ESYS_OK;
    case E_INVALID_ARG:
      return ESYS_ARG;
    case -3:
      return ESYS_NOTREADY;
    case -1:
    default:
      return ESYS_FAIL;
  }
}

static int sysWait(ExecutionState *state)
{
  int timeout = (int)state->ebx;

  if(timeout < 0)
    return (likely(pauseThread(currentThread)) == E_OK) ? ESYS_OK : ESYS_FAIL;
  else if(timeout > 0)
  {
    switch(sleepThread(currentThread, timeout))
    {
      case E_OK:
        return ESYS_OK;
      case E_INVALID_ARG:
        return ESYS_ARG;
      default:
        return ESYS_FAIL;
    }
  }
  else
  {
    if(currentThread->threadState == RUNNING)
      currentThread->threadState = READY;

    return ESYS_OK;
  }
}

int sysReceive(ExecutionState *state)
{
  pid_t localPort = (pid_t)state->ebx;
  tid_t senderTid = (tid_t)state->ecx;
  int blocking = (int)state->edx;

  switch(receiveMessage(currentThread, localPort, senderTid, blocking))
  {
    case E_OK:
      return ESYS_OK;
    case E_INVALID_ARG:
      return ESYS_ARG;
    case E_BLOCK:
      return ESYS_NOTREADY;
    default:
      return ESYS_FAIL;
  }

  return ESYS_FAIL;
}

/*
static int sysCreate(tcb_t *tcb, ...)
{
  if(tcb->tid != INIT_SERVER_TID)
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

      if(likely(registerIrq(args->handler, args->irqNum) == 0))
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
      else if(unlikely(queueEnqueue(&runQueues[newTcb->priority], newTcb->tid, newTcb) != E_OK))
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
          port->owner = tcb;

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
  if(tcb->tid != INIT_SERVER_TID)
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

      args->tcb->addrSpace = targetTcb->rootPageMap;
      args->tcb->status = targetTcb->threadState;
      args->tcb->exHandler = targetTcb->exHandler;
      args->tcb->priority = targetTcb->priority;
      args->tcb->state.eax = targetTcb->execState.eax;
      args->tcb->state.ebx = targetTcb->execState.ebx;
      args->tcb->state.ecx = targetTcb->execState.ecx;
      args->tcb->state.edx = targetTcb->execState.edx;
      args->tcb->state.esi = targetTcb->execState.esi;
      args->tcb->state.edi = targetTcb->execState.edi;
      args->tcb->state.ebp = targetTcb->execState.ebp;
      args->tcb->state.esp = targetTcb->execState.userEsp;
      args->tcb->state.eip = targetTcb->execState.eip;
      args->tcb->state.eflags = targetTcb->execState.eflags;

      return ESYS_OK;
    }
    default:
      return ESYS_NOTIMPL;
  }

  return ESYS_FAIL;
}

static int sysUpdate(tcb_t *tcb, ...)
{
  if(tcb->tid != INIT_SERVER_TID)
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
//      args->tcb->rootPageMap = tcb->rootPageMap;

      targetTcb->exHandler = args->tcb->exHandler;

      if(targetTcb->threadState != args->tcb->status)
      {
        switch(targetTcb->threadState)
        {
          case READY:
            if(detachRunQueue(targetTcb) != E_OK)
              return ESYS_FAIL;
            break;
          case WAIT_FOR_SEND:
            if(detachReceiveQueue(targetTcb, targetTcb->waitPort) != E_OK)
              return ESYS_FAIL;
            break;
          case WAIT_FOR_RECV:
            if(detachSendQueue(targetTcb, targetTcb->waitPort) != E_OK)
              return ESYS_FAIL;
            break;
          case SLEEPING:
            if(queueRemoveLast(&timerQueue, targetTcb->tid, NULL) != E_OK)
              return ESYS_FAIL;
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

      targetTcb->execState.eax = args->tcb->state.eax;
      targetTcb->execState.ebx = args->tcb->state.ebx;
      targetTcb->execState.ecx = args->tcb->state.ecx;
      targetTcb->execState.edx = args->tcb->state.edx;
      targetTcb->execState.esi = args->tcb->state.esi;
      targetTcb->execState.edi = args->tcb->state.edi;
      targetTcb->execState.ebp = args->tcb->state.ebp;
      targetTcb->execState.userEsp = args->tcb->state.esp;
      targetTcb->execState.eip = args->tcb->state.eip;
      targetTcb->execState.eflags = args->tcb->state.eflags;

      return ESYS_OK;
    }
    default:
      return ESYS_NOTIMPL;
  }

  return ESYS_NOTIMPL;
}

static int sysDestroy(tcb_t *tcb, ...)
{
  if(tcb->tid != INIT_SERVER_TID)
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
      port_t *irqHandler = getPort(irqHandlers[args->irqNum]);

      if(args->irqNum < 0 || args->irqNum >= NUM_IRQS || !irqHandler || irqHandler->owner != tcb)
        return ESYS_FAIL;
      else
      {
        unregisterIrq(args->irqNum);
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
      port_t *port;

      if(treeRemove(&portTree, args->port, (void **)&port) != E_OK)
        return ESYS_FAIL;
      else
      {
        free(port);
        return ESYS_OK;
      }
    }
    default:
      return ESYS_NOTIMPL;
  }

  return ESYS_NOTIMPL;
}
*/
// Handles a system call

void _syscall( ExecutionState *execState )
{
  int retval;

  if((unsigned)execState->eax >= sizeof(syscallTable))
    retval = ESYS_BADCALL;
  else
    retval =  syscallTable[(unsigned)execState->eax](execState);

  execState->eax = (dword)retval;
}
