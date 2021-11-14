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
#include <kernel/interrupt.h>
#include <oslib.h>
#include <kernel/bits.h>
#include <kernel/syscall.h>
#include <os/syscalls.h>
#include <os/msg/kernel.h>
#include <os/msg/init.h>
#include <stdnoreturn.h>

typedef struct SyscallArgs {
  union {
    struct {
      uint8_t syscallNum;
      uint8_t byte1;

      union {
        uint16_t word;
        struct {
          uint8_t byte2;
          uint8_t byte3;
        };
      };
    } subArg;
    uint32_t syscallArg;
  };

  uint32_t arg1;
  uint32_t returnAddress;
  uint32_t arg2;
  uint32_t arg3;
  uint32_t arg4;
  uint32_t userStack;
  uint32_t eflags;
} syscall_args_t;

noreturn void sysenter(void) NAKED;

static int sysReceive(syscall_args_t args);
static int sysSend(syscall_args_t args);
static int sysSendAndReceive(syscall_args_t args);

static int sysCreateThread(syscall_args_t args);
static int sysDestroyThread(syscall_args_t args);
static int sysReadThread(syscall_args_t args);
static int sysUpdateThread(syscall_args_t args);

static int sysGetPageMappings(syscall_args_t args);
static int sysSetPageMappings(syscall_args_t args);

int (*syscallTable[])(syscall_args_t) =
{
  NULL,
  NULL,
  sysSend,
  sysReceive,
  sysSendAndReceive,
  sysGetPageMappings,
  sysSetPageMappings,
  sysCreateThread,
  sysDestroyThread,
  sysReadThread,
  sysUpdateThread
};

// arg1 - virt
// arg2 - count
// arg3 - addrSpace
// arg4 - mappings
// subArg.word - level

static int sysGetPageMappings(syscall_args_t args) {
#define VIRT_VAR		args.arg1
#define VIRT        (addr_t)VIRT_VAR
#define COUNT       (size_t)args.arg2
#define ADDR_SPACE_VAR args.arg3
#define ADDR_SPACE  (addr_t)ADDR_SPACE_VAR
#define MAPPINGS    (struct PageMapping *)args.arg3
#define LEVEL       (unsigned int)args.subArg.word

  struct PageMapping *mappings = MAPPINGS;
  size_t i;

  if(!mappings)
    return ESYS_FAIL;

  if(ADDR_SPACE == CURRENT_ROOT_PMAP) {
    if(LEVEL == 2) {
      cr3_t cr3 = {
        .value = getCR3()
      };

      mappings->physAddr = (paddr_t)(cr3.value & CR3_BASE_MASK);
      mappings->flags = 0;

      if(cr3.pcd)
        mappings->flags |= PM_UNCACHED;

      if(cr3.pwt)
        mappings->flags |= PM_WRITETHRU;

      return 1;
    }
    else
      ADDR_SPACE_VAR = (addr_t)getRootPageMap();
  }

  switch(LEVEL ) {
    case 0:
      for(i = 0; i < COUNT && VIRT < KERNEL_VSTART;
          i++, VIRT_VAR += PAGE_SIZE, mappings++)
      {
        pde_t pde = readPDE(PDE_INDEX(VIRT), ADDR_SPACE);

        if(!pde.isPresent)
          RET_MSG((int )i, "Page directory is not present.");

        pte_t pte = readPTE(PTE_INDEX(VIRT), PDE_BASE(pde));

        if(!pte.isPresent) {
          mappings->blockNum = pte.value >> 1;
          mappings->flags |= PM_UNMAPPED;
        }
        else {
          mappings->physAddr = PTE_BASE(pte);
          mappings->flags = PM_PAGE_SIZED;

          if(!pte.isReadWrite)
            mappings->flags |= PM_READ_ONLY;

          if(!pte.isUser)
            mappings->flags |= PM_KERNEL;

          mappings->flags |= pte.available << PM_AVAIL_OFFSET;

          if(pte.dirty)
            mappings->flags |= PM_DIRTY;

          if(pte.pcd)
            mappings->flags |= PM_UNCACHED;

          if(pte.pwt)
            mappings->flags |= PM_WRITETHRU;

          if(pte.accessed)
            mappings->flags |= PM_ACCESSED;
        }
      }
      break;
    case 1:
      for(i = 0; i < COUNT && VIRT < KERNEL_VSTART; i++, VIRT_VAR +=
      PAGE_TABLE_SIZE, mappings++)
      {
        pde_t pde = readPDE(PDE_INDEX(VIRT), ADDR_SPACE);

        if(!pde.isPresent) {
          mappings->blockNum = pde.value >> 1;
          mappings->flags |= PM_UNMAPPED;
        }
        else {
          mappings->physAddr = getPdeBase(pde);
          mappings->flags = 0;

          if(!pde.isReadWrite)
            mappings->flags |= PM_READ_ONLY;

          if(!pde.isUser)
            mappings->flags |= PM_KERNEL;

          if(pde.isLargePage) {
            pmap_entry_t pmapEntry = {
              .pde = pde
            };

            mappings->flags |= PM_PAGE_SIZED;
            mappings->flags |= pmapEntry.largePde.available << PM_AVAIL_OFFSET;

            if(pmapEntry.largePde.dirty)
              mappings->flags |= PM_DIRTY;
          }
          else
            mappings->flags |= ((pde.available2 << 4) | pde.available)
                << PM_AVAIL_OFFSET;

          if(pde.pcd)
            mappings->flags |= PM_UNCACHED;

          if(pde.pwt)
            mappings->flags |= PM_WRITETHRU;

          if(pde.accessed)
            mappings->flags |= PM_ACCESSED;
        }
      }
      break;
    case 2: {
      mappings->physAddr = (paddr_t)ADDR_SPACE;
      mappings->flags = 0;

      return 1;
    }
    default:
      RET_MSG(ESYS_FAIL, "Invalid page map level.");
  }

  return (int)i;
#undef VIRT
#undef VIRT_VAR
#undef COUNT
#undef ADDR_SPACE
#undef ADDR_SPACE_VAR
#undef MAPPINGS
#undef LEVEL
}

// arg1 - virt
// arg2 - count
// arg3 - addrSpace
// arg4 - mappings
// subArg.word - level

static int sysSetPageMappings(syscall_args_t args) {
#define VIRT_VAR		args.arg1
#define VIRT        (addr_t)VIRT_VAR
#define COUNT       (size_t)args.arg2
#define ADDR_SPACE_VAR args.arg3
#define ADDR_SPACE  (paddr_t)ADDR_SPACE_VAR
#define MAPPINGS    (struct PageMapping *)args.arg3
#define LEVEL				(unsigned int)args.subArg.word

  size_t i;
  struct PageMapping *mappings = MAPPINGS;

  if(!mappings)
    return ESYS_FAIL;

  if(ADDR_SPACE == CURRENT_ROOT_PMAP)
    ADDR_SPACE_VAR = (paddr_t)getRootPageMap();

  switch(LEVEL ) {
    case 0:
      for(i = 0; i < COUNT && VIRT < KERNEL_VSTART;
          i++, VIRT_VAR += PAGE_SIZE, mappings++)
      {
        pte_t pte = {
          .value = 0
        };
        pde_t pde = readPDE(PDE_INDEX(VIRT), ADDR_SPACE);

        if(!pde.isPresent)
          RET_MSG((int )i, "PDE is not present for address");

        if(IS_FLAG_SET(mappings->flags, PM_KERNEL))
          RET_MSG((int )i, "Cannot set page with kernel access privilege.");

        if(IS_FLAG_SET(mappings->flags, PM_UNMAPPED)) {
          if(IS_FLAG_SET(mappings->physAddr, 0x80000000u))
            RET_MSG((int )i, "Tried to set invalid block number for PTE.");
          else
            pte.value = mappings->blockNum << 1;
        }
        else {
          uint32_t availBits = ((mappings->flags & PM_AVAIL_MASK)
              >> PM_AVAIL_OFFSET);
          pte.isPresent = 1;

          if(mappings->physAddr >= 1048576u)
            RET_MSG((int )i, "Tried to write invalid frame to PTE.");
          else if(availBits & ~0x07u)
            RET_MSG((int )i, "Cannot set available bits (overflow).");

          pte.base = mappings->physAddr;
          pte.isReadWrite = !IS_FLAG_SET(mappings->flags, PM_READ_ONLY);
          pte.isUser = 1;
          pte.dirty = IS_FLAG_SET(mappings->flags, PM_DIRTY);
          pte.accessed = IS_FLAG_SET(mappings->flags, PM_ACCESSED);
          pte.pat = IS_FLAG_SET(mappings->flags,
              PM_WRITECOMB)
                    && !IS_FLAG_SET(mappings->flags, PM_UNCACHED);
          pte.pcd = IS_FLAG_SET(mappings->flags, PM_UNCACHED);
          pte.pwt = IS_FLAG_SET(mappings->flags, PM_WRITETHRU);
          pte.global = IS_FLAG_SET(mappings->flags, PM_STICKY);
          pte.available = availBits;
        }

        if(IS_ERROR(writePTE(PTE_INDEX(VIRT), pte, ADDR_SPACE)))
          RET_MSG((int )i, "Unable to write to PTE.");

        invalidatePage(VIRT);
      }
      break;
    case 1:
      for(i = 0; i < COUNT && VIRT < KERNEL_VSTART; i++, VIRT_VAR +=
      PAGE_TABLE_SIZE, mappings++)
      {
        pmap_entry_t pmapEntry = {
          .value = 0
        };

        if(IS_FLAG_SET(mappings->flags, PM_KERNEL))
          RET_MSG((int )i, "Cannot set page with kernel access privilege.");

        if(IS_FLAG_SET(mappings->flags, PM_UNMAPPED)) {
          if(IS_FLAG_SET(mappings->physAddr, 0x80000000u))
            RET_MSG((int )i, "Tried to set invalid block number for PDE.");
          else
            pmapEntry.value = mappings->blockNum << 1;
        }
        else {
          pmapEntry.pde.isPresent = 1;

          pmapEntry.pde.isReadWrite = !IS_FLAG_SET(mappings->flags,
                                                   PM_READ_ONLY);
          pmapEntry.pde.isUser = 1;
          pmapEntry.pde.accessed = IS_FLAG_SET(mappings->flags, PM_ACCESSED);
          pmapEntry.pde.pcd = IS_FLAG_SET(mappings->flags, PM_UNCACHED);
          pmapEntry.pde.pwt = IS_FLAG_SET(mappings->flags, PM_WRITETHRU);

          if(IS_FLAG_SET(mappings->flags, PM_PAGE_SIZED)) {
            uint32_t availBits = ((mappings->flags & PM_AVAIL_MASK)
                >> PM_AVAIL_OFFSET);

            if(mappings->physAddr >= 262144u)
              RET_MSG((int )i, "Tried to write invalid frame to PDE.");
            else if(availBits & ~0x07u)
              RET_MSG((int )i, "Cannot set available bits (overflow).");

            pmapEntry.largePde.isLargePage = 1;
            pmapEntry.largePde.dirty = IS_FLAG_SET(mappings->flags, PM_DIRTY);
            pmapEntry.largePde.global = IS_FLAG_SET(mappings->flags, PM_STICKY);
            pmapEntry.largePde.pat = IS_FLAG_SET(mappings->flags,
                PM_WRITECOMB)
                                     && !IS_FLAG_SET(mappings->flags,
                                                     PM_UNCACHED);
            pmapEntry.largePde.available = availBits;
            pmapEntry.largePde.baseLower = mappings->physAddr & 0x3FFu;
            pmapEntry.largePde.baseUpper = mappings->physAddr >> 10u;
            pmapEntry.largePde._resd = 0;
          }
          else {
            uint32_t availBits = ((mappings->flags & PM_AVAIL_MASK)
                >> PM_AVAIL_OFFSET);

            if(mappings->physAddr >= 1048576u)
              RET_MSG((int )i, "Tried to write invalid frame to PDE.");
            else if(availBits & ~0x1fu)
              RET_MSG((int )i, "Cannot set available bits (overflow).");

            pmapEntry.pde.base = mappings->physAddr;
            pmapEntry.pde.available = availBits & 0x0Fu;
            pmapEntry.pde.available2 = availBits >> 4;
            pmapEntry.pde.isLargePage = 0;
          }
        }

        if(IS_ERROR(writePDE(PDE_INDEX(VIRT), pmapEntry.pde, ADDR_SPACE)))
          RET_MSG((int )i, "Unable to write to PDE.");
      }
      break;
    default:
      return ESYS_FAIL;
  }

  return (int)i;
#undef VIRT
#undef VIRT_VAR
#undef COUNT
#undef ADDR_SPACE
#undef ADDR_SPACE_VAR
#undef MAPPINGS
#undef LEVEL
}

// arg1 - entry
// arg2 - addrSpace
// arg3 - stackTop

static int sysCreateThread(syscall_args_t args) {
#define ENTRY	(void *)args.arg1
#define ADDR_SPACE (addr_t)args.arg2
#define STACK_TOP (void *)args.arg3

  if(/*getTid(currentThread) == INIT_SERVER_TID*/1) {
    tcb_t *newTcb = createThread(ENTRY, ADDR_SPACE, STACK_TOP);
    return newTcb ? (int)getTid(newTcb) : ESYS_FAIL;
  }
  else
    RET_MSG(
        ESYS_PERM,
        "Calling thread doesn't have permission to execute this system call.");

#undef ENTRY
#undef ADDR_SPACE
#undef STACK_TOP
}

// arg1 - tid

static int sysDestroyThread(syscall_args_t args) {
#define TID (tid_t)args.arg1

  if(1) {
    tcb_t *tcb = getTcb(TID);
    return tcb && !IS_ERROR(releaseThread(tcb)) ? ESYS_OK : ESYS_FAIL;
  }
  else
    RET_MSG(
        ESYS_PERM,
        "Calling thread doesn't have permission to execute this system call.");

#undef TID
}

// arg1 - tid
// arg2 - flags
// arg3 - info

static int sysReadThread(syscall_args_t args) {
#define TID (tid_t)args.arg1
#define FLAGS (unsigned int)args.arg2
#define INFO (thread_info_t *)args.arg3

  if(1) {
    thread_info_t *info = INFO;
    tcb_t *tcb = TID == NULL_TID ? getCurrentThread() : getTcb(TID);

    if(!tcb)
      return ESYS_ARG;

    if(IS_FLAG_SET(FLAGS, TF_STATUS)) {
      info->status = tcb->threadState;

      if(tcb->threadState == WAIT_FOR_SEND || tcb->threadState == WAIT_FOR_RECV)
        info->waitTid = tcb->waitTid;
    }

    // todo: Need to handle TF_REG_STATE flag

    if(IS_FLAG_SET(FLAGS, TF_PRIORITY))
      info->priority = tcb->priority;

    if(IS_FLAG_SET(FLAGS, TF_PMAP))
      info->rootPageMap = tcb->rootPageMap;

    if(IS_FLAG_SET(FLAGS, TF_XSAVE_STATE))
      memcpy(&info->xsaveState, &tcb->xsaveState, sizeof tcb->xsaveState);

    return ESYS_OK;
  }
  else
    RET_MSG(
        ESYS_PERM,
        "Calling thread doesn't have permission to execute this system call.");

  return ESYS_FAIL;

#undef TID
#undef FLAGS
#undef INFO
}

// arg1 - tid
// arg2 - flags
// arg3 - info

static int sysUpdateThread(syscall_args_t args) {
#define TID (tid_t)args.arg1
#define FLAGS (unsigned int)args.arg2
#define INFO (thread_info_t *)args.arg3

  if(0)
    RET_MSG(
        ESYS_PERM,
        "Calling thread doesn't have permission to execute this system call.");

  thread_info_t *info = INFO;
  tcb_t *tcb = TID == NULL_TID ? getCurrentThread() : getTcb(TID);

  if(!tcb || tcb->threadState == INACTIVE)
    RET_MSG(ESYS_ARG, "The specified thread doesn't exist");

  if(IS_FLAG_SET(FLAGS, TF_PMAP)) {
    tcb->rootPageMap = info->rootPageMap;

    initializeRootPmap(tcb->rootPageMap);
  }

  if(IS_FLAG_SET(FLAGS, TF_XSAVE_STATE))
    memcpy(&tcb->xsaveState, &info->xsaveState, sizeof tcb->xsaveState);

  if(IS_FLAG_SET(FLAGS, TF_REG_STATE)) {
    tcb->userExecState.eax = info->state.eax;
    tcb->userExecState.ebx = info->state.ebx;
    tcb->userExecState.ecx = info->state.ecx;
    tcb->userExecState.edx = info->state.edx;
    tcb->userExecState.esi = info->state.esi;
    tcb->userExecState.edi = info->state.edi;
    tcb->userExecState.ebp = info->state.ebp;
    tcb->userExecState.userEsp = info->state.esp;
    tcb->userExecState.eflags = info->state.eflags;

    tcb->userExecState.cs = info->state.cs;
    tcb->userExecState.userSS = info->state.ss;

    // todo: FS and GS also need to be set

    //__asm__("mov %0, %%fs" :: "m"(info->state.fs));

    switchContext(tcb, 0);

    // Does not return
  }

  return ESYS_OK;
#undef TID
#undef FLAGS
#undef INFO
}

// arg1 - recipient
// arg2 - subject
// arg3 - flags

static int sysSend(syscall_args_t args) {
#define RECIPIENT (tid_t)args.arg1
#define SUBJECT (uint32_t)args.arg2
#define FLAGS (unsigned int)args.arg3

  tcb_t *currentThread = getCurrentThread();

  if(IS_FLAG_SET(FLAGS, MSG_KERNEL))
    return ESYS_ARG;

  currentThread->userExecState.userEsp = args.userStack;
  currentThread->userExecState.eip = args.returnAddress;

  switch(sendMessage(currentThread, RECIPIENT, SUBJECT, FLAGS)) {
    case E_OK:
      return ESYS_OK;
    case E_INVALID_ARG:
      return ESYS_ARG;
    case E_BLOCK:
      return ESYS_NOTREADY;
    case E_FAIL:
    default:
      return ESYS_FAIL;
  }
#undef RECIPIENT
#undef SUBJECT
#undef FLAGS
}

// arg1 - sender
// arg2 - flags

int sysReceive(syscall_args_t args) {
#define SENDER (tid_t)args.arg1
#define FLAGS (unsigned int)args.arg2
  tcb_t *currentThread = getCurrentThread();

  currentThread->userExecState.userEsp = args.userStack;
  currentThread->userExecState.eip = args.returnAddress;

  switch(receiveMessage(currentThread, SENDER, FLAGS)) {
    case E_OK:
      return ESYS_OK;
    case E_INVALID_ARG:
      return ESYS_ARG;
    case E_BLOCK:
      return ESYS_NOTREADY;
    case E_FAIL:
    default:
      return ESYS_FAIL;
  }
#undef SENDER
#undef FLAGS
}

// arg1 - targets (replier [upper 16-bits] / recipient [lower 16-bits])
// arg2 - subject
// arg3 - sendFlags
// arg4 - recvFlags

static int sysSendAndReceive(syscall_args_t args) {
#define TARGETS (uint32_t)args.arg1
#define SUBJECT (uint32_t)args.arg2
#define SEND_FLAGS (unsigned int)args.arg3
#define RECV_FLAGS (unsigned int)args.arg4
  tcb_t *currentThread = getCurrentThread();

  if(IS_FLAG_SET(args.arg3, MSG_KERNEL))
    return ESYS_ARG;

  currentThread->userExecState.userEsp = args.userStack;
  currentThread->userExecState.eip = args.returnAddress;

  switch(sendAndReceiveMessage(currentThread, (tid_t)(TARGETS & 0xFFFFu),
                               (tid_t)(TARGETS >> 16), SUBJECT, SEND_FLAGS,
                               RECV_FLAGS)) {
    case E_OK:
      return ESYS_OK;
    case E_INVALID_ARG:
      return ESYS_ARG;
    case E_BLOCK:
      return ESYS_NOTREADY;
    case E_FAIL:
    default:
      return ESYS_FAIL;
  }
#undef TARGETS
#undef SUBJECT
#undef SEND_FLAGS
#undef RECV_FLAGS
}

void sysenter(void) {
  __asm__ __volatile__
  (
      "pushf\n"
      "push %ebp\n"
      "push %edi\n"
      "push %esi\n"
      "push %edx\n"
      "push %ecx\n"
      "push %ebx\n"
      "push %eax\n"
      "and  $0xFF, %eax\n"
      "lea  syscallTable, %ebx\n"
      "call *(%ebx,%eax,4)\n"
      "add  $4, %esp\n"
      "pop %ebx\n"
      "add $8, %esp\n"
      "pop %esi\n"
      "pop %edi\n"
      "pop %ebp\n"
      "popf\n"
      "mov %ebp, %edx\n"
      "sti\n"		// STI must be the second to last instruction
                // to prevent interrupts from firing while in kernel mode
      "sysexit\n"
  );
}
