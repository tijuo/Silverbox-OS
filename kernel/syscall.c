#include <kernel/memory.h>
#include <kernel/mm.h>
#include <kernel/debug.h>
#include <kernel/schedule.h>
#include <kernel/error.h>
#include <kernel/message.h>
#include <os/msg/message.h>
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
      uint8_t syscall_num;
      uint8_t byte1;

      union {
        uint16_t word;
        struct {
          uint8_t byte2;
          uint8_t byte3;
        };
      };
    } sub_arg;
    uint32_t syscall_arg;
  };

  uint32_t arg1;
  uint32_t arg2;
  uint32_t return_address;
  uint32_t arg3;
  uint32_t arg4;
  uint32_t user_stack;
  uint32_t eflags;
} syscall_args_t;

union thread_targets {
  struct {
    tid_t recipient;
    tid_t replier;
  };
  long int targets;
};

noreturn void syscall_entry(void) NAKED;

static long int handle_sys_receive(tid_t sender, long int flags);
static long int handle_sys_send(tid_t recipient, long int subject, long int flags);
static long int handle_sys_send_and_receive(union thread_targets targets,
    long int subject, long int send_flags,
    long int recv_flags);

static long int handle_sys_create_thread(void *entry, paddr_t addr_space,
    void *stack_top);
static long int handle_sys_destroy_thread(tid_t tid);
static long int handle_sys_read_thread(tid_t tid, thread_info_t *info);
static long int handle_sys_update_thread(tid_t tid, long int, thread_info_t *info);

static long int handle_sys_grant(void);
static long int handle_sys_revoke(void);
static long int handle_sys_poll(long int mask);
static long int handle_sys_wait(long int mask);

/*
 static int handle_sys_get_page_mappings(syscall_args_t args);
 static int handle_sys_set_page_mappings(syscall_args_t args);
 */

void *syscall_table[] = {
  handle_sys_grant,
  handle_sys_revoke,
  handle_sys_send,
  handle_sys_receive,
  handle_sys_send_and_receive,
  NULL,
  NULL,
  handle_sys_create_thread,
  handle_sys_destroy_thread,
  handle_sys_read_thread,
  handle_sys_update_thread,
  handle_sys_poll,
  handle_sys_wait
};

long int handle_sys_grant() {
  return ESYS_NOTIMPL;
}

long int handle_sys_revoke() {
  return ESYS_NOTIMPL;
}

long int handle_sys_poll(long int mask) {
  tcb_t *current_thread = get_current_thread();
  long int ret_val = current_thread->pending_events & mask;

  current_thread->pending_events &= ~mask;

  return ret_val;
}

long int handle_sys_wait(long int mask) {
  tcb_t *current_thread = get_current_thread();
  long int ret_val = current_thread->pending_events & mask;

  if(!ret_val) {
    pause_thread(current_thread);

    tcb_t *new_thread = schedule(get_current_processor());

    if(new_thread != current_thread) {
      // todo: rip and user rsp need to be saved. all other registers can be clobbered
    }

    switch_context(new_thread);
  } else
    current_thread->pending_events &= ~mask;

  return ret_val;
}

/*
 // arg1 - virt
 // arg2 - count
 // arg3 - addr_space
 // arg4 - mappings
 // sub_arg.word - level

 int handle_sys_get_page_mappings(syscall_args_t args) {
 #define VIRT_VAR args.arg1
 #define VIRT        (addr_t)VIRT_VAR
 #define COUNT       (size_t)args.arg2
 #define ADDR_SPACE_VAR args.arg3
 #define ADDR_SPACE  (addr_t)ADDR_SPACE_VAR
 #define MAPPINGS    (struct PageMapping *)args.arg4
 #define LEVEL       (unsigned int)args.sub_arg.word

 struct PageMapping *mappings = MAPPINGS;
 size_t i;

 if(!mappings)
 return ESYS_FAIL;

 if(ADDR_SPACE == CURRENT_ROOT_PMAP) {
 if(LEVEL == 2) {
 cr3_t cr3 = {
 .value = get_cr3()
 };

 mappings->phys_frame = (pframe_t)cr3.base;
 mappings->flags = 0;

 if(cr3.pcd)
 mappings->flags |= PM_UNCACHED;

 if(cr3.pwt)
 mappings->flags |= PM_WRITETHRU;

 return 1;
 }
 else
 ADDR_SPACE_VAR = (addr_t)get_root_page_map();
 }

 kprintf("sys_get_page_mappings: virt-0x%x count-0x%x frame-0x%x flags:-0x%x level: %d\n",
 VIRT, COUNT, mappings->phys_frame, mappings->flags, LEVEL);

 switch(LEVEL ) {
 case 0:
 for(i = 0; i < COUNT && VIRT < KERNEL_VSTART;
 i++, VIRT_VAR += PAGE_SIZE, mappings++)
 {
 pde_t pde = read_pde(PDE_INDEX(VIRT), ADDR_SPACE);

 if(!pde.is_present)
 RET_MSG((int )i, "Page directory is not present.");

 pte_t pte = read_pte(PTE_INDEX(VIRT), PDE_BASE(pde));

 if(!pte.is_present) {
 mappings->block_num = pte.value >> 1;
 mappings->flags |= PM_UNMAPPED;
 }
 else {
 mappings->phys_frame = pte.base;
 mappings->flags = PM_PAGE_SIZED;

 if(!pte.is_read_write)
 mappings->flags |= PM_READ_ONLY;

 if(!pte.is_user)
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
 pde_t pde = read_pde(PDE_INDEX(VIRT), ADDR_SPACE);

 if(!pde.is_present) {
 mappings->block_num = pde.value >> 1;
 mappings->flags |= PM_UNMAPPED;
 }
 else {
 mappings->phys_frame = get_pde_frame_number(pde);
 mappings->flags = 0;

 if(!pde.is_read_write)
 mappings->flags |= PM_READ_ONLY;

 if(!pde.is_user)
 mappings->flags |= PM_KERNEL;

 if(pde.is_large_page) {
 pmap_entry_t pmap_entry = {
 .pde = pde
 };

 mappings->flags |= PM_PAGE_SIZED;
 mappings->flags |= pmap_entry.large_pde.available << PM_AVAIL_OFFSET;

 if(pmap_entry.large_pde.dirty)
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
 if(COUNT) {
 mappings->phys_frame = PADDR_TO_PFRAME(ADDR_SPACE);
 mappings->flags = 0;

 return 1;
 }
 else
 return 0;
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
 // arg3 - addr_space
 // arg4 - mappings
 // sub_arg.word - level

 int handle_sys_set_page_mappings(syscall_args_t args) {
 #define VIRT_VAR   args.arg1
 #define VIRT        (addr_t)VIRT_VAR
 #define COUNT       (size_t)args.arg2
 #define ADDR_SPACE_VAR args.arg3
 #define ADDR_SPACE  (uint32_t)ADDR_SPACE_VAR
 #define MAPPINGS    (struct PageMapping *)args.arg4
 #define LEVEL        (unsigned int)args.sub_arg.word

 size_t i;
 struct PageMapping *mappings = MAPPINGS;
 pframe_t pframe;
 bool is_array = false;

 if(!mappings)
 return ESYS_FAIL;

 if(ADDR_SPACE == CURRENT_ROOT_PMAP)
 ADDR_SPACE_VAR = (uint32_t)get_root_page_map();

 kprintf("sys_set_page_mappings: virt-0x%x count-0x%x frame-0x%x flags:-0x%x level: %d\n",
 VIRT, COUNT, mappings->phys_frame, mappings->flags, LEVEL);

 switch(LEVEL ) {
 case 0:
 for(i = 0; i < COUNT && VIRT < KERNEL_VSTART; i++, VIRT_VAR += PAGE_SIZE)
 {
 pte_t pte = {
 .value = 0
 };

 pde_t pde = read_pde(PDE_INDEX(VIRT), ADDR_SPACE);

 if(!pde.is_present)
 RET_MSG((int )i, "PDE is not present for address");

 if(IS_FLAG_SET(mappings->flags, PM_KERNEL))
 RET_MSG((int )i, "Cannot set page with kernel access privilege.");

 pte_t old_pte = read_pte(PTE_INDEX(VIRT), PDE_BASE(pde));

 if(old_pte.is_present && !IS_FLAG_SET(mappings->flags, PM_OVERWRITE)) {
 kprintf("Mapping for 0x%x in address space 0x%x already exists!\n", VIRT, ADDR_SPACE);
 RET_MSG((int )i, "Attempted to overwrite page mapping.");
 }

 if(IS_FLAG_SET(mappings->flags, PM_UNMAPPED)) {
 if((is_array ? pframe : mappings->block_num) >= 0x80000000u)
 RET_MSG((int )i, "Tried to set invalid block number for PTE.");
 else
 pte.value = (is_array ? pframe : mappings->block_num) << 1;
 }
 else {
 uint32_t avail_bits = ((mappings->flags & PM_AVAIL_MASK)
 >> PM_AVAIL_OFFSET);
 pte.is_present = 1;

 if((is_array ? pframe : mappings->phys_frame) >= (1 << 20))
 RET_MSG((int )i, "Tried to write invalid frame to PTE.");
 else if(avail_bits & ~0x07u)
 RET_MSG((int )i, "Cannot set available bits (overflow).");

 pte.base = is_array ? pframe : mappings->phys_frame;
 pte.is_read_write = !IS_FLAG_SET(mappings->flags, PM_READ_ONLY);
 pte.is_user = 1;
 pte.dirty = IS_FLAG_SET(mappings->flags, PM_DIRTY);
 pte.accessed = IS_FLAG_SET(mappings->flags, PM_ACCESSED);
 pte.pat = IS_FLAG_SET(mappings->flags,
 PM_WRITECOMB)
 && !IS_FLAG_SET(mappings->flags, PM_UNCACHED);
 pte.pcd = IS_FLAG_SET(mappings->flags, PM_UNCACHED);
 pte.pwt = IS_FLAG_SET(mappings->flags, PM_WRITETHRU);
 pte.global = IS_FLAG_SET(mappings->flags, PM_STICKY);
 pte.available = avail_bits;

 if(IS_FLAG_SET(mappings->flags, PM_CLEAR))
 clear_phys_page(PTE_BASE(pte));
 }

 if(IS_ERROR(write_pte(PTE_INDEX(VIRT), pte, PDE_BASE(pde))))
 RET_MSG((int )i, "Unable to write to PTE.");

 invalidate_page(VIRT);

 if(IS_FLAG_SET(mappings->flags, PM_ARRAY)) {
 if(!is_array) {
 is_array = true;
 pframe = mappings->phys_frame;
 }

 pframe++;
 }
 else
 mappings++;
 }
 break;
 case 1:
 for(i = 0; i < COUNT && VIRT < KERNEL_VSTART; i++, VIRT_VAR +=
 PAGE_TABLE_SIZE, mappings++)
 {
 pmap_entry_t pmap_entry = {
 .value = 0
 };

 if(IS_FLAG_SET(mappings->flags, PM_KERNEL))
 RET_MSG((int )i, "Cannot set page with kernel access privilege.");

 pde_t old_pde = read_pde(PDE_INDEX(VIRT), ADDR_SPACE);

 if(old_pde.is_present && !IS_FLAG_SET(mappings->flags, PM_OVERWRITE))
 RET_MSG((int )i, "Attempted to overwrite page mapping.");

 if(IS_FLAG_SET(mappings->flags, PM_UNMAPPED)) {
 if((is_array ? pframe : mappings->block_num) >= 0x80000000u)
 RET_MSG((int )i, "Tried to set invalid block number for PDE.");
 else
 pmap_entry.value = (is_array ? pframe : mappings->block_num) << 1;
 }
 else {
 pmap_entry.pde.is_present = 1;

 pmap_entry.pde.is_read_write = !IS_FLAG_SET(mappings->flags,
 PM_READ_ONLY);
 pmap_entry.pde.is_user = 1;
 pmap_entry.pde.accessed = IS_FLAG_SET(mappings->flags, PM_ACCESSED);
 pmap_entry.pde.pcd = IS_FLAG_SET(mappings->flags, PM_UNCACHED);
 pmap_entry.pde.pwt = IS_FLAG_SET(mappings->flags, PM_WRITETHRU);

 if(IS_FLAG_SET(mappings->flags, PM_PAGE_SIZED)) {
 uint32_t avail_bits = ((mappings->flags & PM_AVAIL_MASK)
 >> PM_AVAIL_OFFSET);

 if((is_array ? pframe : mappings->phys_frame) >= (1 << 28))
 RET_MSG((int )i, "Tried to write invalid frame to PDE.");
 else if(avail_bits & ~0x07u)
 RET_MSG((int )i, "Cannot set available bits (overflow).");

 pmap_entry.large_pde.is_large_page = 1;
 pmap_entry.large_pde.dirty = IS_FLAG_SET(mappings->flags, PM_DIRTY);
 pmap_entry.large_pde.global = IS_FLAG_SET(mappings->flags, PM_STICKY);
 pmap_entry.large_pde.pat = IS_FLAG_SET(mappings->flags,
 PM_WRITECOMB)
 && !IS_FLAG_SET(mappings->flags,
 PM_UNCACHED);
 pmap_entry.large_pde.available = avail_bits;
 pmap_entry.large_pde.base_lower = ((
 is_array ? pframe : mappings->phys_frame)
 >> 10)
 & 0x3FFu;
 pmap_entry.large_pde.base_upper = (
 is_array ? pframe : mappings->phys_frame)
 >> 20u;
 pmap_entry.large_pde._resd = 0;
 }
 else {
 uint32_t avail_bits = ((mappings->flags & PM_AVAIL_MASK)
 >> PM_AVAIL_OFFSET);

 if((is_array ? pframe : mappings->phys_frame) >= (1 << 20))
 RET_MSG((int )i, "Tried to write invalid frame to PDE.");
 else if(avail_bits & ~0x1fu)
 RET_MSG((int )i, "Cannot set available bits (overflow).");

 pmap_entry.pde.base = is_array ? pframe : mappings->phys_frame;
 pmap_entry.pde.available = avail_bits & 0x0Fu;
 pmap_entry.pde.available2 = avail_bits >> 4;
 pmap_entry.pde.is_large_page = 0;

 if(IS_FLAG_SET(mappings->flags, PM_CLEAR))
 clear_phys_page(PDE_BASE(pmap_entry.pde));
 }
 }

 if(IS_ERROR(write_pde(PDE_INDEX(VIRT), pmap_entry.pde, ADDR_SPACE)))
 RET_MSG((int )i, "Unable to write to PDE.");

 if(IS_FLAG_SET(mappings->flags, PM_ARRAY))
 pframe += LARGE_PAGE_SIZE / PAGE_SIZE;
 else
 mappings++;
 }
 break;
 case 2:
 return ESYS_PERM;
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
 */

long int handle_sys_create_thread(void *entry, paddr_t addr_space, void *stack_top) {
  if(1) {
    tcb_t *new_tcb = create_thread(entry, addr_space, stack_top);
    return new_tcb ? (int)get_tid(new_tcb) : ESYS_FAIL;
  } else
    RET_MSG(
      ESYS_PERM,
      "Calling thread doesn't have permission to execute this system call.");
}

// arg1 - tid

long int handle_sys_destroy_thread(tid_t tid) {
  if(1) {
    tcb_t *tcb = get_tcb(tid);
    return tcb && !IS_ERROR(release_thread(tcb)) ? ESYS_OK : ESYS_FAIL;
  } else
    RET_MSG(
      ESYS_PERM,
      "Calling thread doesn't have permission to execute this system call.");
}

long int handle_sys_read_thread(tid_t tid, thread_info_t *info) {
  if(1) {
    tcb_t *tcb = tid == NULL_TID ? get_current_thread() : get_tcb(tid);

    if(!tcb)
      return ESYS_ARG;

    info->status = tcb->thread_state;

    if(tcb->thread_state == WAIT_FOR_SEND || tcb->thread_state == WAIT_FOR_RECV)
      info->wait_tid = tcb->wait_tid;

    info->state.rax = tcb->user_exec_state.rax;
    info->state.rbx = tcb->user_exec_state.rbx;
    info->state.rcx = tcb->user_exec_state.rcx;
    info->state.rdx = tcb->user_exec_state.rdx;
    info->state.rsi = tcb->user_exec_state.rsi;
    info->state.rdi = tcb->user_exec_state.rdi;
    info->state.rbp = tcb->user_exec_state.rbp;
    info->state.rsp = tcb->user_exec_state.rsp;
    info->state.r8 = tcb->user_exec_state.r8;
    info->state.r9 = tcb->user_exec_state.r9;
    info->state.r10 = tcb->user_exec_state.r10;
    info->state.r11 = tcb->user_exec_state.r11;
    info->state.r12 = tcb->user_exec_state.r12;
    info->state.r13 = tcb->user_exec_state.r13;
    info->state.r14 = tcb->user_exec_state.r14;
    info->state.r15 = tcb->user_exec_state.r15;
    info->state.rip = tcb->user_exec_state.rip;
    info->state.rflags = tcb->user_exec_state.rflags;
    info->state.cs = tcb->user_exec_state.cs;
    info->state.ds = tcb->user_exec_state.ds;
    info->state.es = tcb->user_exec_state.es;
    info->state.fs = tcb->user_exec_state.fs;
    info->state.gs = tcb->user_exec_state.gs;
    info->state.ss = tcb->user_exec_state.ss;

    info->priority = tcb->priority;
    info->root_pmap = tcb->root_pmap;

    if(tcb->thread_state == RUNNING) {
      for(size_t i = 0; i < MAX_PROCESSORS; i++) {
        if(processors[i].running_thread == tcb) {
          info->current_processor_id = (uint8_t)i;
          break;
        }
      }

      info->tid = get_tid(tcb);

      info->pending_events = tcb->pending_events;
      info->event_mask = tcb->event_mask;

      info->cap_table = tcb->cap_table;
      info->cap_table_entry_count = tcb->cap_table_capacity;
      info->parent = tcb->parent;

      info->exception_handler = tcb->ex_handler;

      info->xsave_state = tcb->xsave_state;
      return ESYS_OK;
    } else
      RET_MSG(
        ESYS_PERM,
        "Calling thread doesn't have permission to execute this system call.");

    return ESYS_FAIL;
  }
}

long int handle_sys_update_thread(tid_t tid, long int flags, thread_info_t *info) {
  if(0)
    RET_MSG(
      ESYS_PERM,
      "Calling thread doesn't have permission to execute this system call.");

  tcb_t *tcb = tid == NULL_TID ? get_current_thread() : get_tcb(tid);

  if(!tcb || tcb->thread_state == INACTIVE)
    RET_MSG(ESYS_ARG, "The specified thread doesn't exist");

  // todo: wait for a RUNNING thread to finish before continuing

  if(tcb->thread_state == RUNNING)
    RET_MSG(ESYS_FAIL, "The specified thread is currently running.");

  if(IS_FLAG_SET(flags, TF_STATUS)) {

    switch(info->status) {
      case READY:
      case PAUSED:
        break;
      default:
        RET_MSG(ESYS_ARG, "Unable to change thread status");
    }

    remove_thread_from_list(tcb);

    if(info->status == READY)
      start_thread(tcb);

    tcb->thread_state = info->status;
  }

  if(IS_FLAG_SET(flags, TF_ROOT_PMAP)) {
    tcb->root_pmap = info->root_pmap;

    initialize_root_pmap(tcb->root_pmap);
  }

  if(IS_FLAG_SET(flags, TF_EXT_REG_STATE))
    memcpy(&tcb->xsave_state, &info->xsave_state, sizeof tcb->xsave_state);

  if(IS_FLAG_SET(flags, TF_REG_STATE)) {
    tcb->user_exec_state.rax = info->state.rax;
    tcb->user_exec_state.rbx = info->state.rbx;
    tcb->user_exec_state.rcx = info->state.rcx;
    tcb->user_exec_state.rdx = info->state.rdx;
    tcb->user_exec_state.rsi = info->state.rsi;
    tcb->user_exec_state.rdi = info->state.rdi;
    tcb->user_exec_state.r8 = info->state.r8;
    tcb->user_exec_state.r9 = info->state.r9;
    tcb->user_exec_state.r10 = info->state.r10;
    tcb->user_exec_state.r11 = info->state.r11;
    tcb->user_exec_state.r12 = info->state.r12;
    tcb->user_exec_state.r13 = info->state.r13;
    tcb->user_exec_state.r14 = info->state.r14;
    tcb->user_exec_state.r15 = info->state.r15;

    tcb->user_exec_state.rbp = info->state.rbp;
    tcb->user_exec_state.rsp = info->state.rsp;
    tcb->user_exec_state.rflags = info->state.rflags;

    tcb->user_exec_state.cs = info->state.cs;
    tcb->user_exec_state.ds = info->state.ds;
    tcb->user_exec_state.es = info->state.es;
    tcb->user_exec_state.fs = info->state.fs;
    tcb->user_exec_state.gs = info->state.gs;
    tcb->user_exec_state.ss = info->state.ss;

    if(tcb == get_current_thread())
      // todo: rip and user rsp need to be saved. all other registers can be clobbered

      switch_context(tcb);

    // Does not return
  }

  return ESYS_OK;
}

long int handle_sys_send(tid_t recipient, long int subject, long int flags) {
  tcb_t *current_thread = get_current_thread();

  if(IS_FLAG_SET(flags, MSG_KERNEL))
    return ESYS_ARG;

  // todo: rip and user rsp need to be saved. all other registers can be clobbered

  switch(send_message(current_thread, recipient, subject, flags)) {
    case E_OK:
      return ESYS_OK;
    case E_INVALID_ARG:
      return ESYS_ARG;
    case E_BLOCK:
      return ESYS_NOTREADY;
    case E_INTERRUPT:
      return ESYS_INT;
    case E_FAIL:
    default:
      return ESYS_FAIL;
  }
}

long int handle_sys_receive(tid_t sender, long int flags) {
  tcb_t *current_thread = get_current_thread();

  // todo: rip and user rsp need to be saved. all other registers can be clobbered

  switch(receive_message(current_thread, sender, flags)) {
    case E_OK:
      return ESYS_OK;
    case E_INVALID_ARG:
      return ESYS_ARG;
    case E_BLOCK:
      return ESYS_NOTREADY;
    case E_INTERRUPT:
      return ESYS_INT;
    case E_FAIL:
    default:
      return ESYS_FAIL;
  }
}

long int handle_sys_send_and_receive(union thread_targets targets, long int subject,
                                     long int send_flags, long int recv_flags) {
  tcb_t *current_thread = get_current_thread();

  if(IS_FLAG_SET(send_flags, MSG_KERNEL))
    return ESYS_ARG;

  // todo: rip and user rsp need to be saved. all other registers can be clobbered

  switch(send_and_receive_message(current_thread, targets.recipient,
                                  targets.replier, subject, send_flags,
                                  recv_flags)) {
    case E_OK:
      return ESYS_OK;
    case E_INVALID_ARG:
      return ESYS_ARG;
    case E_BLOCK:
      return ESYS_NOTREADY;
    case E_INTERRUPT:
      return ESYS_INT;
    case E_FAIL:
    default:
      return ESYS_FAIL;
  }
}

void syscall_entry(void) {
  __asm__(
    "cmp $12, %rax\n"
    "ja 2f\n"
    "mov %rsp, tss+4\n"
    "lea kernel_stack_top, %rsp\n"
    "xchg %rbx, %rcx\n"
    "push %rbx\n"
    "push %r11\n"
    "sub $8, %rsp\n"
    "lea syscall_table, %rbx\n"
    "lea (%rbx,%rax,8), %rax\n"
    "call *%rax\n"
    "add $8, %rsp\n"
    "pop %r11\n"
    "pop %rcx\n"
    "jmp 1f\n"
    "2:\n"
    "movq $-4, %rax\n"
    "1:\n"
    "sysretq\n");
}

/*
 void sysenter_entry(void) {
 // This is aligned as long as the frame pointer isn't pushed and no other arguments are pushed
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
 "mov  $0x10, %bx\n"
 "mov  %bx, %ds\n"
 "mov  %bx, %es\n"
 "and  $0xFF, %eax\n"
 "lea  syscall_table, %ebx\n"
 "call *(%ebx,%eax,4)\n"
 "mov $0x23, %bx\n"
 "mov %bx, %ds\n"
 "mov %bx, %es\n"
 "add  $4, %esp\n"
 "pop %ebx\n"
 "add $4, %esp\n"
 "pop %edx\n"      // edx is the return address that the user saved
 "pop %esi\n"
 "pop %edi\n"
 "pop %ebp\n"
 "popf\n"
 "mov %ebp, %ecx\n" // ebp is the stack pointer that the user saved
 "sti\n"    // STI must be the second to last instruction
 // to prevent interrupts from firing while in kernel mode
 "sysexit\n"
 );
 }

 */
