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
static int sysCall(ExecutionState *state);

// Privileged call
static int sysMap(ExecutionState *state);
static int sysUnmap(ExecutionState *state);

static int sysCreateThread(ExecutionState *state);
static int sysDestroyThread(ExecutionState *state);
static int sysReadThread(ExecutionState *state);
static int sysUpdateThread(ExecutionState *state);

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
  sysSend,
  sysReceive,
  sysCall,
  sysCall,
  sysMap,
  sysUnmap,
  sysCreateThread,
  sysDestroyThread,
  sysReadThread,
  sysUpdateThread,
  sysEoi,
  sysIrqWait,
  sysBindIrq,
  sysUnbindIrq
};

static int sysMap(ExecutionState *state)
{
  u32 addrSpace = (u32)state->ebx;
  addr_t vAddr = (addr_t)state->ecx;
  pframe_t pframe = (pframe_t)state->edx;
  int flags = (int)state->edi;
  size_t numPages = (size_t)state->esi;
  int invalidate = 0;

  if(getTid(currentThread) == INIT_SERVER_TID)
  {
    pde_t pde;
    pte_t pte;

    if(addrSpace == NULL)
    {
      addrSpace = getCR3();
      invalidate = 1;
    }

    for(; numPages--; vAddr += (flags & PM_LARGE_PAGE) ? PAGE_TABLE_SIZE : PAGE_SIZE,
                      pframe += (flags & PM_LARGE_PAGE) ? PAGE_TABLE_SIZE / PAGE_SIZE : 1)
    {
      if(readPmapEntry(addrSpace, PDE_INDEX(vAddr), &pde) != E_OK)
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

        if(writePmapEntry(addrSpace, PDE_INDEX(vAddr), &pde) != E_OK)
        {
          if(!(flags & PM_LARGE_PAGE))
            freePageFrame(addr);
          return ESYS_FAIL;
        }
        else if(flags & PM_LARGE_PAGE)
        {
          if(invalidate)
            invalidatePage(vAddr & ~(PAGE_TABLE_SIZE-1));

          continue;
        }
      }

      if(readPmapEntry((paddr_t)(pde.base << 12), PTE_INDEX(vAddr), &pte) != E_OK)
        return ESYS_FAIL;

      if(pte.present)
        return ESYS_FAIL;
      else
      {
        pte.base = (u32)pframe;
        pte.rwPriv = (flags & PM_READ_WRITE) == PM_READ_WRITE;
        pte.usPriv = pte.present = 1;
        pde.pwt = (flags & PM_WRITE_THROUGH) == PM_WRITE_THROUGH;
        pde.pcd = (flags & PM_NOT_CACHED) == PM_NOT_CACHED;

        if(writePmapEntry((paddr_t)(pde.base << 12), PTE_INDEX(vAddr), &pte) != E_OK)
          return ESYS_FAIL;

        if(invalidate)
          invalidatePage(vAddr & ~(PAGE_SIZE-1));

        continue;
      }
    }

    return ESYS_OK;
  }
  else
    return ESYS_PERM;

  return ESYS_FAIL;
}

static int sysUnmap(ExecutionState *state)
{
  u32 addrSpace = (u32)state->ebx;
  addr_t addr = (addr_t)state->ecx;
  size_t numPages = (size_t)state->edx;

  if(getTid(currentThread) == INIT_SERVER_TID)
  {
    pde_t pde;
    pte_t pte;

    if(addrSpace == NULL)
      addrSpace = getCR3();

    while(numPages--)
    {
      if(readPmapEntry(addrSpace, PDE_INDEX(addr), &pde) != E_OK)
        return ESYS_FAIL;
      else if(!pde.present)
        return ESYS_FAIL;

      if(pde.pageSize)
      {
        pde.present = 0;

        if(writePmapEntry(addrSpace, PDE_INDEX(addr), &pde) != E_OK)
          return ESYS_FAIL;

        addr += PAGE_TABLE_SIZE;
      }
      else
      {
        if(readPmapEntry((paddr_t)(pde.base << 12), PTE_INDEX(addr), &pte) != E_OK || !pte.present)
          return ESYS_FAIL;

        pte.present = 0;

        if(writePmapEntry((paddr_t)(pde.base << 12), PTE_INDEX(addr), &pte) != E_OK)
          return ESYS_FAIL;

        addr += PAGE_SIZE;
      }
    }

    return ESYS_OK;
  }
  else
    return ESYS_PERM;
}

static int sysCreateThread(ExecutionState *state)
{
  addr_t entry = (addr_t)state->ebx;
  u32 addrSpace = (u32)state->ecx;
  addr_t stackTop = (addr_t)state->edx;

  if(getTid(currentThread) == INIT_SERVER_TID)
  {
    if(addrSpace == NULL)
      addrSpace = getCR3() & ~0xFFF;

    tcb_t *newTcb = createThread(entry, (paddr_t)addrSpace, stackTop);
    return newTcb ? (int)getTid(newTcb) : ESYS_FAIL;
  }
  else
    return ESYS_PERM;
}

static int sysDestroyThread(ExecutionState *state)
{
  if(getTid(currentThread) == INIT_SERVER_TID)
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
  int flags = (int)state->ecx;
  thread_info_t *info = (thread_info_t *)state->edx;

  if(getTid(currentThread) == INIT_SERVER_TID)
  {
    tcb_t *tcb = tid == NULL_TID ? currentThread : getTcb(tid);

    if(!tcb)
      return ESYS_ARG;

    if(flags & TF_STATUS)
    {
      info->status = tcb->threadState;

      if(tcb->threadState == WAIT_FOR_SEND || tcb->threadState == WAIT_FOR_RECV)
        info->waitTid = tcb->waitTid;
    }

    if(flags & TF_REG_STATE)
    {
      if(tcb == currentThread)
      {
        info->state.eax = state->eax;
        info->state.ebx = state->ebx;
        info->state.ecx = state->ecx;
        info->state.edx = state->edx;
        info->state.esi = state->esi;
        info->state.edi = state->edi;
        info->state.esp = state->userEsp;
        info->state.ebp = state->ebp;
        info->state.eflags = state->eflags;
        info->state.eip = state->eip;
      }
      else
      {
        info->state.eax = tcb->execState.eax;
        info->state.ebx = tcb->execState.ebx;
        info->state.ecx = tcb->execState.ecx;
        info->state.edx = tcb->execState.edx;
        info->state.esi = tcb->execState.esi;
        info->state.edi = tcb->execState.edi;
        info->state.esp = tcb->execState.userEsp;
        info->state.ebp = tcb->execState.ebp;
        info->state.eflags = tcb->execState.eflags;
        info->state.eip = tcb->execState.eip;
      }
    }

    if(flags & TF_PMAP)
      info->rootPageMap = tcb->rootPageMap;

    if(flags & TF_PRIORITY)
      info->priority = tcb->priority;

    return ESYS_OK;
  }
  else
    return ESYS_PERM;

  return ESYS_FAIL;
}

static int sysUpdateThread(ExecutionState *state)
{
  tid_t tid = (tid_t)state->ebx;
  int flags = (int)state->ecx;
  thread_info_t *info = (thread_info_t *)state->edx;

  if(getTid(currentThread) == INIT_SERVER_TID)
  {
    tcb_t *tcb = tid == NULL_TID ? currentThread : getTcb(tid);

    if(!tcb)
      return ESYS_FAIL;

    if(flags & TF_STATUS)
    {
      tcb->threadState = info->status;

      if(info->status == READY)
        attachRunQueue(tcb);

      if(tcb->threadState == WAIT_FOR_SEND || tcb->threadState == WAIT_FOR_RECV)
        info->waitTid = tcb->waitTid;
    }

    if(flags & TF_REG_STATE)
    {
      if(tcb == currentThread)
      {
        state->eax = info->state.eax;
        state->ebx = info->state.eax;
        state->ecx = info->state.eax;
        state->edx = info->state.eax;
        state->esi = info->state.eax;
        state->edi = info->state.eax;
        state->userEsp = info->state.esp;
        state->ebp = info->state.ebp;
        state->eflags = info->state.eflags;
        state->eip = info->state.eip;
      }
      else
      {
        tcb->execState.eax = info->state.eax;
        tcb->execState.ebx = info->state.eax;
        tcb->execState.ecx = info->state.eax;
        tcb->execState.edx = info->state.eax;
        tcb->execState.esi = info->state.eax;
        tcb->execState.edi = info->state.eax;
        tcb->execState.userEsp = info->state.esp;
        tcb->execState.ebp = info->state.ebp;
        tcb->execState.eflags = info->state.eflags;
        tcb->execState.eip = info->state.eip;
      }
    }

    if(flags & TF_PMAP)
    {
      tcb->rootPageMap = info->rootPageMap;

      initializeRootPmap(tcb->rootPageMap);
    }

    if(flags & TF_PRIORITY)
      setPriority(tcb, info->priority);

    return ESYS_OK;
  }
  else
    return ESYS_PERM;

  return ESYS_FAIL;
}

static int sysExit(ExecutionState *state)
{
  pem_t message =
  {
    .subject = EXIT_MSG,
    .who = getTid(currentThread),
    .errorCode = (int)state->ebx
  };

  sendExceptionMessage(currentThread, INIT_SERVER_TID, &message);

  return ESYS_OK;
}

static int sysBindIrq(ExecutionState *state)
{
  tcb_t *tcb = getTcb((tid_t)state->ebx);
  int irq = (int)state->ecx;

  if(getTid(currentThread) == INIT_SERVER_TID)
    return registerIrq(tcb, irq) == E_OK ? ESYS_OK : ESYS_FAIL;
  else
    return ESYS_PERM;
}

static int sysUnbindIrq(ExecutionState *state)
{
  if(getTid(currentThread) == INIT_SERVER_TID)
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

static int sysSend(ExecutionState *state)
{
  tid_t remoteTid = (tid_t)(state->eax >> 16);
  int isBlocking = (int)((state->eax & 0xFF) == SYS_SEND_WAIT);

  switch(sendMessage(currentThread, state, remoteTid, isBlocking, 0))
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

static int sysCall(ExecutionState *state)
{
  tid_t remoteTid = (tid_t)(state->eax >> 16);
  int isBlocking = (int)((state->eax & 0xFF) == SYS_CALL_WAIT);

  switch(sendMessage(currentThread, state ,remoteTid, isBlocking, 1))
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
  tid_t senderTid = (tid_t)(state->eax >> 16);
  int isBlocking = (int)((state->eax & 0xFF) == SYS_RECEIVE_WAIT);

  switch(receiveMessage(currentThread, state, senderTid, isBlocking))
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

// Handles a system call

void _syscall( ExecutionState *execState )
{
  int retval;
  unsigned int callIndex = execState->eax & 0xFF;

  retval = callIndex >= sizeof(syscallTable) ? ESYS_BADCALL : syscallTable[callIndex](execState);

  switch(callIndex)
  {
    case SYS_SEND:
    case SYS_SEND_WAIT:
    case SYS_CALL:
    case SYS_CALL_WAIT:
    case SYS_RECEIVE:
    case SYS_RECEIVE_WAIT:
      if(retval != ESYS_OK)
        execState->eax = (dword)retval;
      else
        execState->eax = (execState->eax & 0xFFFFFF00) | ESYS_OK;
      break;
    default:
      execState->eax = (dword)retval;
      break;
  }
}
