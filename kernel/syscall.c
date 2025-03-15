#include <kernel/bits.h>
#include <kernel/debug.h>
#include <kernel/error.h>
#include <kernel/interrupt.h>
#include <kernel/lowlevel.h>
#include <kernel/memory.h>
#include <kernel/message.h>
#include <kernel/mm.h>
#include <kernel/schedule.h>
#include <kernel/syscall.h>
#include <kernel/thread.h>
#include <os/msg/init.h>
#include <os/msg/kernel.h>
#include <os/msg/message.h>
#include <os/syscalls.h>
#include <oslib.h>
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

noreturn void sysenter_entry(void) NAKED;

static int handle_sys_receive(syscall_args_t args);
static int handle_sys_send(syscall_args_t args);
static int handle_sys_send_and_reecive(syscall_args_t args);

static int handle_sys_create_thread(syscall_args_t args);
static int handle_sys_destroy_thread(syscall_args_t args);
static int handle_sys_read_thread(syscall_args_t args);
static int handle_sys_update_thread(syscall_args_t args);

static int handle_sys_get_page_mappings(syscall_args_t args);
static int handle_sys_set_page_mappings(syscall_args_t args);

UNUSED static int (*syscall_table[])(syscall_args_t) = {
    NULL,
    NULL,
    handle_sys_send,
    handle_sys_receive,
    handle_sys_send_and_reecive,
    handle_sys_get_page_mappings,
    handle_sys_set_page_mappings,
    handle_sys_create_thread,
    handle_sys_destroy_thread,
    handle_sys_read_thread,
    handle_sys_update_thread
};

// arg1 - virt
// arg2 - count
// arg3 - addr_space
// arg4 - mappings
// sub_arg.word - level

int handle_sys_get_page_mappings(syscall_args_t args)
{
#define VIRT_VAR args.arg1
#define VIRT (addr_t) VIRT_VAR
#define COUNT (size_t)args.arg2
#define ADDR_SPACE_VAR args.arg3
#define ADDR_SPACE (addr_t) ADDR_SPACE_VAR
#define MAPPINGS (struct PageMapping*)args.arg4
#define LEVEL (unsigned int)args.sub_arg.word

    struct PageMapping* mappings = MAPPINGS;
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
        } else
            ADDR_SPACE_VAR = (addr_t)get_root_page_map();
    }

    kprintf("sys_get_page_mappings: virt-0x%x count-0x%x frame-0x%x flags:-0x%x level: %d\n",
        VIRT, COUNT, mappings->phys_frame, mappings->flags, LEVEL);

    switch(LEVEL) {
        case 0:
            for(i = 0; i < COUNT && VIRT < KERNEL_VSTART;
                i++, VIRT_VAR += PAGE_SIZE, mappings++) {
                pde_t pde = read_pde(PDE_INDEX(VIRT), ADDR_SPACE);

                if(!pde.is_present)
                    RET_MSG((int)i, "Page directory is not present.");

                pte_t pte = read_pte(PTE_INDEX(VIRT), PDE_BASE(pde));

                if(!pte.is_present) {
                    mappings->block_num = pte.value >> 1;
                    mappings->flags |= PM_UNMAPPED;
                } else {
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
            for(i = 0; i < COUNT && VIRT < KERNEL_VSTART; i++, VIRT_VAR += PAGE_TABLE_SIZE, mappings++) {
                pde_t pde = read_pde(PDE_INDEX(VIRT), ADDR_SPACE);

                if(!pde.is_present) {
                    mappings->block_num = pde.value >> 1;
                    mappings->flags |= PM_UNMAPPED;
                } else {
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
                    } else {
                        mappings->flags |= ((pde.available2 ? (1u << 4) : 0) | pde.available) << PM_AVAIL_OFFSET;
                    }

                    if(pde.pcd)
                        mappings->flags |= PM_UNCACHED;

                    if(pde.pwt)
                        mappings->flags |= PM_WRITETHRU;

                    if(pde.accessed)
                        mappings->flags |= PM_ACCESSED;
                }
            }
            break;
        case 2:
        {
            if(COUNT) {
                mappings->phys_frame = PADDR_TO_PFRAME(ADDR_SPACE);
                mappings->flags = 0;

                return 1;
            } else
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

int handle_sys_set_page_mappings(syscall_args_t args)
{
#define VIRT_VAR args.arg1
#define VIRT (addr_t) VIRT_VAR
#define COUNT (size_t)args.arg2
#define ADDR_SPACE_VAR args.arg3
#define ADDR_SPACE (uint32_t)ADDR_SPACE_VAR
#define MAPPINGS (struct PageMapping*)args.arg4
#define LEVEL (unsigned int)args.sub_arg.word

    size_t i;
    struct PageMapping* mappings = MAPPINGS;
    pframe_t pframe = 0;
    bool is_array = false;

    if(!mappings)
        return ESYS_FAIL;

    if(ADDR_SPACE == CURRENT_ROOT_PMAP)
        ADDR_SPACE_VAR = (uint32_t)get_root_page_map();

    kprintf("sys_set_page_mappings: virt-0x%x count-0x%x frame-0x%x flags:-0x%x level: %d\n",
        VIRT, COUNT, mappings->phys_frame, mappings->flags, LEVEL);

    if(IS_FLAG_SET(mappings->flags, PM_ARRAY)) {
        is_array = true;
        pframe = mappings->phys_frame;
    }

    switch(LEVEL) {
        case 0:
            for(i = 0; i < COUNT && VIRT < KERNEL_VSTART; i++, VIRT_VAR += PAGE_SIZE) {
                pte_t pte = {
                    .value = 0
                };

                pde_t pde = read_pde(PDE_INDEX(VIRT), ADDR_SPACE);

                if(!pde.is_present)
                    RET_MSG((int)i, "PDE is not present for address");

                if(IS_FLAG_SET(mappings->flags, PM_KERNEL))
                    RET_MSG((int)i, "Cannot set page with kernel access privilege.");

                pte_t old_pte = read_pte(PTE_INDEX(VIRT), PDE_BASE(pde));

                if(old_pte.is_present && !IS_FLAG_SET(mappings->flags, PM_OVERWRITE)) {
                    kprintf("Mapping for 0x%x in address space 0x%x already exists!\n", VIRT, ADDR_SPACE);
                    RET_MSG((int)i, "Attempted to overwrite page mapping.");
                }

                if(IS_FLAG_SET(mappings->flags, PM_UNMAPPED)) {
                    if((is_array ? pframe : mappings->block_num) >= 0x80000000u)
                        RET_MSG((int)i, "Tried to set invalid block number for PTE.");
                    else
                        pte.value = (is_array ? pframe : mappings->block_num) << 1;
                } else {
                    uint32_t avail_bits = ((mappings->flags & PM_AVAIL_MASK) >> PM_AVAIL_OFFSET);
                    pte.is_present = 1;

                    if((is_array ? pframe : mappings->phys_frame) >= (1u << 20))
                        RET_MSG((int)i, "Tried to write invalid frame to PTE.");
                    else if(avail_bits & ~0x07u)
                        RET_MSG((int)i, "Cannot set available bits (overflow).");

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
                    RET_MSG((int)i, "Unable to write to PTE.");

                invalidate_page(VIRT);

                if(is_array) {
                    pframe++;
                } else {
                    mappings++;
                }
            }
            break;
        case 1:
            for(i = 0; i < COUNT && VIRT < KERNEL_VSTART; i++, VIRT_VAR += PAGE_TABLE_SIZE, mappings++) {
                pmap_entry_t pmap_entry = {
                    .value = 0
                };

                if(IS_FLAG_SET(mappings->flags, PM_KERNEL))
                    RET_MSG((int)i, "Cannot set page with kernel access privilege.");

                pde_t old_pde = read_pde(PDE_INDEX(VIRT), ADDR_SPACE);

                if(old_pde.is_present && !IS_FLAG_SET(mappings->flags, PM_OVERWRITE))
                    RET_MSG((int)i, "Attempted to overwrite page mapping.");

                if(IS_FLAG_SET(mappings->flags, PM_UNMAPPED)) {
                    if((is_array ? pframe : mappings->block_num) >= 0x80000000u)
                        RET_MSG((int)i, "Tried to set invalid block number for PDE.");
                    else
                        pmap_entry.value = (is_array ? pframe : mappings->block_num) << 1;
                } else {
                    pmap_entry.pde.is_present = 1;

                    pmap_entry.pde.is_read_write = !IS_FLAG_SET(mappings->flags,
                        PM_READ_ONLY);
                    pmap_entry.pde.is_user = 1;
                    pmap_entry.pde.accessed = IS_FLAG_SET(mappings->flags, PM_ACCESSED);
                    pmap_entry.pde.pcd = IS_FLAG_SET(mappings->flags, PM_UNCACHED);
                    pmap_entry.pde.pwt = IS_FLAG_SET(mappings->flags, PM_WRITETHRU);

                    if(IS_FLAG_SET(mappings->flags, PM_PAGE_SIZED)) {
                        uint32_t avail_bits = ((mappings->flags & PM_AVAIL_MASK) >> PM_AVAIL_OFFSET);

                        if((is_array ? pframe : mappings->phys_frame) >= (1u << 28))
                            RET_MSG((int)i, "Tried to write invalid frame to PDE.");
                        else if(avail_bits & ~0x07u)
                            RET_MSG((int)i, "Cannot set available bits (overflow).");

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
                        pmap_entry.large_pde.base_upper = (is_array ? pframe : mappings->phys_frame) >> 20u;
                        pmap_entry.large_pde._resd = 0;
                    } else {
                        uint32_t avail_bits = ((mappings->flags & PM_AVAIL_MASK) >> PM_AVAIL_OFFSET);

                        if((is_array ? pframe : mappings->phys_frame) >= (1u << 20))
                            RET_MSG((int)i, "Tried to write invalid frame to PDE.");
                        else if(avail_bits & ~0x1fu)
                            RET_MSG((int)i, "Cannot set available bits (overflow).");

                        pmap_entry.pde.base = is_array ? pframe : mappings->phys_frame;
                        pmap_entry.pde.available = avail_bits & 0x0Fu;
                        pmap_entry.pde.available2 = avail_bits >> 4;
                        pmap_entry.pde.is_large_page = 0;

                        if(IS_FLAG_SET(mappings->flags, PM_CLEAR))
                            clear_phys_page(PDE_BASE(pmap_entry.pde));
                    }
                }

                if(IS_ERROR(write_pde(PDE_INDEX(VIRT), pmap_entry.pde, ADDR_SPACE)))
                    RET_MSG((int)i, "Unable to write to PDE.");

                if(is_array) {
                    pframe += LARGE_PAGE_SIZE / PAGE_SIZE;
                } else {
                    mappings++;
                }
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

// arg1 - entry
// arg2 - addr_space
// arg3 - stack_top

int handle_sys_create_thread(syscall_args_t args)
{
#define ENTRY (void*)args.arg1
#define ADDR_SPACE (addr_t) args.arg2
#define STACK_TOP (void*)args.arg3

    if(1) {
        tcb_t* new_tcb = create_thread(ENTRY, ADDR_SPACE, STACK_TOP);
        return new_tcb ? (int)get_tid(new_tcb) : ESYS_FAIL;
    } else
        RET_MSG(
            ESYS_PERM,
            "Calling thread doesn't have permission to execute this system call.");

#undef ENTRY
#undef ADDR_SPACE
#undef STACK_TOP
}

// arg1 - tid

int handle_sys_destroy_thread(syscall_args_t args)
{
#define TID (tid_t) args.arg1

    if(1) {
        tcb_t* tcb = get_tcb(TID);
        return tcb && !IS_ERROR(release_thread(tcb)) ? ESYS_OK : ESYS_FAIL;
    } else
        RET_MSG(
            ESYS_PERM,
            "Calling thread doesn't have permission to execute this system call.");

#undef TID
}

// arg1 - tid
// arg2 - info

int handle_sys_read_thread(syscall_args_t args)
{
#define TID (tid_t) args.arg1
#define INFO (thread_info_t*)args.arg2

    if(1) {
        thread_info_t* info = INFO;
        tcb_t* tcb = TID == NULL_TID ? get_current_thread() : get_tcb(TID);

        if(!tcb)
            return ESYS_ARG;

        info->status = tcb->thread_state;

        if(tcb->thread_state == WAIT_FOR_SEND || tcb->thread_state == WAIT_FOR_RECV)
            info->wait_tid = tcb->wait_tid;

        info->state.eax = tcb->user_exec_state.eax;
        info->state.ebx = tcb->user_exec_state.ebx;
        info->state.ecx = tcb->user_exec_state.ecx;
        info->state.edx = tcb->user_exec_state.edx;
        info->state.esi = tcb->user_exec_state.esi;
        info->state.edi = tcb->user_exec_state.edi;
        info->state.ebp = tcb->user_exec_state.ebp;
        info->state.esp = tcb->user_exec_state.user_esp;
        info->state.eip = tcb->user_exec_state.eip;
        info->state.eflags = tcb->user_exec_state.eflags;
        info->state.cs = tcb->user_exec_state.cs;
        info->state.ds = tcb->user_exec_state.ds;
        info->state.es = tcb->user_exec_state.es;
        info->state.fs = tcb->user_exec_state.fs;
        info->state.gs = tcb->user_exec_state.gs;
        info->state.ss = tcb->user_exec_state.user_ss;

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

            info->capability_table = tcb->cap_table;
            info->capability_table_len = tcb->cap_table_size;

            info->exception_handler = tcb->ex_handler;

            info->xsave_state = tcb->xsave_state;
            info->xsave_state_bits = tcb->xsave_state_bits;
            info->xsave_state_len = tcb->xsave_state_len;

            return ESYS_OK;
        } else
            RET_MSG(
                ESYS_PERM,
                "Calling thread doesn't have permission to execute this system call.");

        return ESYS_FAIL;

#undef TID
#undef INFO
    }
}
// arg1 - tid
// arg2 - flags
// arg3 - info

int handle_sys_update_thread(syscall_args_t args)
{
#define TID (tid_t) args.arg1
#define FLAGS (unsigned int)args.arg2
#define INFO (thread_info_t*)args.arg3

    if(0)
        RET_MSG(
            ESYS_PERM,
            "Calling thread doesn't have permission to execute this system call.");

    thread_info_t* info = INFO;
    tcb_t* tcb = TID == NULL_TID ? get_current_thread() : get_tcb(TID);

    if(!tcb || tcb->thread_state == INACTIVE)
        RET_MSG(ESYS_ARG, "The specified thread doesn't exist");

    if(IS_FLAG_SET(FLAGS, TF_STATUS)) {

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

    if(IS_FLAG_SET(FLAGS, TF_ROOT_PMAP)) {
        tcb->root_pmap = info->root_pmap;
        initialize_root_pmap(tcb->root_pmap);
    }

    if(IS_FLAG_SET(FLAGS, TF_EXT_REG_STATE))
        memcpy(&tcb->xsave_state, &info->xsave_state, sizeof tcb->xsave_state);

    if(IS_FLAG_SET(FLAGS, TF_REG_STATE)) {
        tcb->user_exec_state.eax = info->state.eax;
        tcb->user_exec_state.ebx = info->state.ebx;
        tcb->user_exec_state.ecx = info->state.ecx;
        tcb->user_exec_state.edx = info->state.edx;
        tcb->user_exec_state.esi = info->state.esi;
        tcb->user_exec_state.edi = info->state.edi;
        tcb->user_exec_state.ebp = info->state.ebp;
        tcb->user_exec_state.user_esp = info->state.esp;
        tcb->user_exec_state.eflags = info->state.eflags;

        tcb->user_exec_state.cs = info->state.cs;
        tcb->user_exec_state.user_ss = info->state.ss;

        // todo: FS and GS also need to be set

        //__asm__("mov %0, %%fs" :: "m"(info->state.fs));

        switch_context(tcb, 0);

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

int handle_sys_send(syscall_args_t args)
{
#define RECIPIENT (tid_t) args.arg1
#define SUBJECT (uint32_t)args.arg2
#define FLAGS (unsigned int)args.arg3

    tcb_t* current_thread = get_current_thread();

    if(IS_FLAG_SET(FLAGS, MSG_KERNEL))
        return ESYS_ARG;

    current_thread->user_exec_state.user_esp = args.user_stack;
    current_thread->user_exec_state.eip = args.return_address;

    switch(send_message(current_thread, RECIPIENT, SUBJECT, FLAGS)) {
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
#undef RECIPIENT
#undef SUBJECT
#undef FLAGS
}

// arg1 - sender
// arg2 - flags

int handle_sys_receive(syscall_args_t args)
{
#define SENDER (tid_t) args.arg1
#define FLAGS (unsigned int)args.arg2
    tcb_t* current_thread = get_current_thread();

    current_thread->user_exec_state.user_esp = args.user_stack;
    current_thread->user_exec_state.eip = args.return_address;

    switch(receive_message(current_thread, SENDER, FLAGS)) {
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
#undef SENDER
#undef FLAGS
}

// arg1 - targets (replier [upper 16-bits] / recipient [lower 16-bits])
// arg2 - subject
// arg3 - send_flags
// arg4 - recv_flags

int handle_sys_send_and_reecive(syscall_args_t args)
{
#define TARGETS (uint32_t)args.arg1
#define SUBJECT (uint32_t)args.arg2
#define SEND_FLAGS (unsigned int)args.arg3
#define RECV_FLAGS (unsigned int)args.arg4
    tcb_t* current_thread = get_current_thread();

    if(IS_FLAG_SET(args.arg3, MSG_KERNEL))
        return ESYS_ARG;

    current_thread->user_exec_state.user_esp = args.user_stack;
    current_thread->user_exec_state.eip = args.return_address;

    switch(send_and_receive_message(current_thread, (tid_t)(TARGETS & 0xFFFFu),
        (tid_t)(TARGETS >> 16), SUBJECT, SEND_FLAGS,
        RECV_FLAGS)) {
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
#undef TARGETS
#undef SUBJECT
#undef SEND_FLAGS
#undef RECV_FLAGS
}

/* XXX: Potential security vulnerability: User shouldn't be allow to set arbitrary
   return addresses in ecx register.  */

void sysenter_entry(void)
{
    // This is aligned as long as the frame pointer isn't pushed and no other arguments are pushed
    __asm__ __volatile__(
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
        "pop %edx\n" // edx is the return address that the user saved
        "pop %esi\n"
        "pop %edi\n"
        "pop %ebp\n"
        "popf\n"
        "mov %ebp, %ecx\n" // ebp is the stack pointer that the user saved
        "sti\n" // STI must be the second to last instruction
                // to prevent interrupts from firing while in kernel mode
        "sysexit\n");
}
