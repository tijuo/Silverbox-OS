#include <kernel/bits.h>
#include <kernel/debug.h>
#include <kernel/error.h>
#include <kernel/interrupt.h>
#include <kernel/lowlevel.h>
#include <kernel/memory.h>
#include <kernel/message.h>
#include <kernel/mm.h>
#include <kernel/schedule.h>
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

static int handle_sys_create(syscall_args_t args);
static int handle_sys_read(syscall_args_t args);
static int handle_sys_update(syscall_args_t args);
static int handle_sys_destroy(syscall_args_t args);

static int handle_sys_send(syscall_args_t args);
static int handle_sys_receive(syscall_args_t args);
static int handle_sys_read_page_mappings(SysReadPageMappingsArgs* args);
static int handle_sys_update_page_mappings(SysUpdatePageMappingsArgs* args);

static int handle_sys_create_tcb(SysCreateTcbArgs* args);
static int handle_sys_read_tcb(SysReadTcbArgs* args);
static int handle_sys_update_tcb(SysUpdateTcbArgs* args);
static int handle_sys_destroy_tcb(SysDestroyTcbArgs* args);

static int handle_sys_create_int(SysCreateIntArgs* args);
static int handle_sys_read_int(SysReadIntArgs* args);
static int handle_sys_update_int(SysUpdateIntArgs* args);
static int handle_sys_destroy_int(SysDestroyIntArgs* args);

static int handle_sys_sleep(syscall_args_t args);

#define ARG_RES_TYPE    (SysResource)args.arg1
#define ARG_ARGS        (void *)args.arg2
static int handle_sys_create(syscall_args_t args)
{
    switch(ARG_RES_TYPE) {
        case RES_TCB:
            return handle_sys_create_tcb(ARG_ARGS);
        case RES_INT:
            return handle_sys_create_int(ARG_ARGS);
        case RES_PAGE_MAPPING:
        case RES_CAP:
        default:
            return ESYS_NOTIMPL;
    }
}
#undef ARG_RES_TYPE
#undef ARG_ARGS

#define ARG_RES_TYPE    (SysResource)args.arg1
#define ARG_ARGS        (void *)args.arg2
static int handle_sys_read(syscall_args_t args)
{
    switch(ARG_RES_TYPE) {
        case RES_PAGE_MAPPING:
            return handle_sys_read_page_mappings(ARG_ARGS);
        case RES_INT:
            return handle_sys_read_int(ARG_ARGS);
        case RES_TCB:
            return handle_sys_read_tcb(ARG_ARGS);
        case RES_CAP:
        default:
            return ESYS_NOTIMPL;
    }
}
#undef ARG_RES_TYPE
#undef ARG_ARGS

#define ARG_RES_TYPE    (SysResource)args.arg1
#define ARG_ARGS        (void *)args.arg2
static int handle_sys_update(syscall_args_t args)
{
    switch(ARG_RES_TYPE) {
        case RES_PAGE_MAPPING:
            return handle_sys_update_page_mappings(ARG_ARGS);
        case RES_INT:
            return handle_sys_update_int(ARG_ARGS);
        case RES_TCB:
            return handle_sys_update_tcb(ARG_ARGS);
        case RES_CAP:
        default:
            return ESYS_NOTIMPL;
    }
}
#undef ARG_RES_TYPE
#undef ARG_ARGS

#define ARG_RES_TYPE    (SysResource)args.arg1
#define ARG_ARGS        (void *)args.arg2
static int handle_sys_destroy(syscall_args_t args)
{
    switch(ARG_RES_TYPE) {
        case RES_TCB:
            return handle_sys_destroy_tcb(ARG_ARGS);
        case RES_INT:
            return handle_sys_destroy_int(ARG_ARGS);
        case RES_PAGE_MAPPING:
        case RES_CAP:
        default:
            return ESYS_NOTIMPL;
    }
}
#undef ARG_RES_TYPE
#undef ARG_ARGS

#define ARG_DURATION    (unsigned int)args.arg1
#define ARG_GRANULARITY (int)args.arg2
static int handle_sys_sleep(syscall_args_t args)
{
    tcb_t* current_thread = thread_get_current();
    tcb_t* new_thread = NULL;
    int ret_val;

    if(ARG_DURATION == 0) {
        new_thread = schedule(processor_get_current());    // yield processor to another thread
        ret_val = (new_thread != current_thread) ? ESYS_OK : ESYS_FAIL;
    } else if(ARG_DURATION == SL_INF_DURATION) {
        switch(thread_pause(current_thread)) {
            case E_OK:
                new_thread = schedule(processor_get_current());
                ret_val = ESYS_OK;
                break;
            case E_FAIL:
                ret_val = ESYS_FAIL;
                break;
            case E_DONE:
                PANIC("A paused thread shouldn't be able to call sys_sleep()");
            default:
                UNREACHABLE;
        }
        ret_val = ESYS_FAIL;
    } else {
        switch(thread_sleep(thread_get_current(), ARG_DURATION, ARG_GRANULARITY)) {
            case E_OK:
                new_thread = schedule(processor_get_current());
                ret_val = ESYS_OK;
                break;
            case E_FAIL:
                ret_val = ESYS_FAIL;
                break;
            case E_DONE:
                PANIC("A paused thread shouldn't be able to call sys_sleep()");
            default:
                UNREACHABLE;
        }
    }

    if(ret_val == ESYS_OK) {
        thread_switch_context(new_thread, true);
    }

    return ret_val;
}
#undef ARG_DURATION
#undef ARG_GRANULARITY

// arg1 - virt
// arg2 - count
// arg3 - addr_space
// arg4 - mappings
// sub_arg.word - level

static int handle_sys_read_page_mappings(SysReadPageMappingsArgs* args)
{
    PageMapping* mappings = args->mappings;
    size_t i;

    if(!mappings)
        return ESYS_FAIL;

    if(args->addr_space == CURRENT_ROOT_PMAP) {
        if(args->level == 2) {
            cr3_t cr3 = {
                .value = get_cr3()
            };

            mappings->phys_frame = (paddr_t)cr3.base;
            mappings->flags = 0;

            if(cr3.pcd)
                mappings->flags |= PM_UNCACHED;

            if(cr3.pwt)
                mappings->flags |= PM_WRITETHRU;

            return 1;
        } else
            args->addr_space = (addr_t)get_root_page_map();
    }

    kprintf("sys_get_page_mappings: virt-%#x count-%#x frame-%#x flags:-%#x level: %d\n",
        args->virt, args->count, mappings->phys_frame, mappings->flags, args->level);

    switch(args->level) {
        case 0:
            for(i = 0; i < args->count && args->virt < KERNEL_VSTART;
                i++, args->virt += PAGE_SIZE, mappings++) {
                pde_t pde;

                if(IS_ERROR(read_pde(&pde, PDE_INDEX(args->virt), args->addr_space))) {
                    RET_MSG((int)i, "Unable to read PDE.");
                };

                if(!pde.is_present)
                    RET_MSG((int)i, "Page directory is not present.");

                pte_t pte;

                if(IS_ERROR(read_pte(&pte, PTE_INDEX(args->virt), PDE_BASE(pde)))) {
                    RET_MSG((int)i, "Unable to read PTE.");
                }

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
            for(i = 0; i < args->count && args->virt < KERNEL_VSTART; i++, args->virt += PAGE_TABLE_SIZE, mappings++) {
                pde_t pde;
                
                if(IS_ERROR(read_pde(&pde, PDE_INDEX(args->virt), args->addr_space))) {
                    RET_MSG((int)i, "Unable to read PDE.");
                }

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

                    if(pde.is_page_sized) {
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
            if(args->count) {
                mappings->phys_frame = PADDR_TO_PBASE(args->addr_space);
                mappings->flags = 0;

                return 1;
            } else
                return 0;
        }
        default:
            RET_MSG(ESYS_FAIL, "Invalid page map level.");
    }

    return (int)i;
}

// arg1 - virt
// arg2 - count
// arg3 - addr_space
// arg4 - mappings
// sub_arg.word - level
static int handle_sys_update_page_mappings(SysUpdatePageMappingsArgs* args)
{
    size_t i;
    PageMapping* mappings = args->mappings;
    pbase_t pframe = 0;
    bool is_array = false;

    if(!mappings)
        return ESYS_FAIL;

    if(args->addr_space == CURRENT_ROOT_PMAP)
        args->addr_space = (uint32_t)get_root_page_map();

    kprintf("sys_set_page_mappings: virt-%#x count-%#x frame-%#x flags:-%#x level: %d\n",
        args->virt, args->count, mappings->phys_frame, mappings->flags, args->level);

    if(IS_FLAG_SET(mappings->flags, PM_ARRAY)) {
        is_array = true;
        pframe = mappings->phys_frame;
    }

    switch(args->level) {
        case 0:
            for(i = 0; i < args->count && args->virt < KERNEL_VSTART; i++, args->virt += PAGE_SIZE) {
                pte_t pte = {
                    .value = 0
                };

                pde_t pde;
                
                if(IS_ERROR(read_pde(&pde, PDE_INDEX(args->virt), args->addr_space))) {
                    RET_MSG((int)i, "Unable to read PDE.");
                }

                if(!pde.is_present)
                    RET_MSG((int)i, "PDE is not present for address");

                if(IS_FLAG_SET(mappings->flags, PM_KERNEL))
                    RET_MSG((int)i, "Cannot set page with kernel access privilege.");

                pte_t old_pte;
                
                if(IS_ERROR(read_pte(&old_pte, PTE_INDEX(args->virt), PDE_BASE(pde)))) {
                    RET_MSG((int)i, "Unable to read PTE.");
                }

                if(old_pte.is_present && !IS_FLAG_SET(mappings->flags, PM_OVERWRITE)) {
                    kprintf("Mapping for %#x in address space %#x already exists!\n", args->virt, args->addr_space);
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

                    if(IS_FLAG_SET(mappings->flags, PM_CLEAR)) {
                        if(clear_phys_frames(PTE_BASE(pte), 1) != 1) {
                            RET_MSG((int)i, "Unable to clear physical frame.");
                        }
                    }
                }

                if(IS_ERROR(write_pte(PTE_INDEX(args->virt), pte, PDE_BASE(pde))))
                    RET_MSG((int)i, "Unable to write to PTE.");

                invalidate_page(args->virt);

                if(is_array) {
                    pframe++;
                } else {
                    mappings++;
                }
            }
            break;
        case 1:
            for(i = 0; i < args->count && args->virt < KERNEL_VSTART; i++, args->virt += PAGE_TABLE_SIZE, mappings++) {
                pmap_entry_t pmap_entry = {
                    .value = 0
                };

                if(IS_FLAG_SET(mappings->flags, PM_KERNEL))
                    RET_MSG((int)i, "Cannot set page with kernel access privilege.");

                pde_t old_pde;
                
                if(IS_ERROR(read_pde(&old_pde, PDE_INDEX(args->virt), args->addr_space))) {
                    RET_MSG((int)i, "Unablo to read PDE.");
                }

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

                        pmap_entry.large_pde.is_page_sized = 1;
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
                        pmap_entry.pde.is_page_sized = 0;

                        if(IS_FLAG_SET(mappings->flags, PM_CLEAR)) {
                            if(clear_phys_frames(PDE_BASE(pmap_entry.pde), 1) != 1) {
                                RET_MSG((int)i, "Unable to clear physical frame.");
                            }
                        }
                    }
                }

                if(IS_ERROR(write_pde(PDE_INDEX(args->virt), pmap_entry.pde, args->addr_space)))
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
}

// arg1 - entry
// arg2 - addr_space
// arg3 - stack_top

static int handle_sys_create_tcb(SysCreateTcbArgs* args)
{
    if(1) {
        tcb_t* new_tcb = thread_create(args->entry, args->addr_space, (void*)args->stack_top);
        return new_tcb ? (int)get_tid(new_tcb) : ESYS_FAIL;
    } else
        RET_MSG(
            ESYS_PERM,
            "Calling thread doesn't have permission to execute this system call.");
}

// arg1 - tid

static int handle_sys_destroy_tcb(SysDestroyTcbArgs* args)
{
    if(1) {
        tcb_t* tcb = get_tcb(args->tid);
        return tcb && !IS_ERROR(thread_release(tcb)) ? ESYS_OK : ESYS_FAIL;
    } else
        RET_MSG(
            ESYS_PERM,
            "Calling thread doesn't have permission to execute this system call.");
}

// arg1 - tid
// arg2 - info

static int handle_sys_read_tcb(SysReadTcbArgs* args)
{
    if(1) {
        thread_info_t* info = args->info;
        tcb_t* tcb = args->tid == NULL_TID ? thread_get_current() : get_tcb(args->tid);

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

            info->xsave_state = (void*)tcb->xsave_state;
            info->xsave_rfbm = tcb->xsave_rfbm;
            info->xsave_state_len = tcb->xsave_state_len;

            return ESYS_OK;
        } else
            RET_MSG(
                ESYS_PERM,
                "Calling thread doesn't have permission to execute this system call.");

        return ESYS_FAIL;
    }
}
// arg1 - tid
// arg2 - flags
// arg3 - info

static int handle_sys_update_tcb(SysUpdateTcbArgs* args)
{
    if(0)
        RET_MSG(
            ESYS_PERM,
            "Calling thread doesn't have permission to execute this system call.");

    thread_info_t* info = args->info;
    tcb_t* tcb = args->tid == NULL_TID ? thread_get_current() : get_tcb(args->tid);

    if(!tcb || tcb->thread_state == INACTIVE)
        RET_MSG(ESYS_ARG, "The specified thread doesn't exist");

    if(IS_FLAG_SET(args->flags, TF_STATUS)) {

        switch(info->status) {
            case READY:
            case PAUSED:
                break;
            default:
                RET_MSG(ESYS_ARG, "Unable to change thread status");
        }

        if(IS_ERROR(thread_remove_from_list(tcb))) {
            RET_MSG(ESYS_FAIL, "Unable to remove thread from its list.");
        }

        if(info->status == READY) {
            if(IS_ERROR(thread_start(tcb))) {
                RET_MSG(ESYS_FAIL, "Unable to start thread.");
            }
        }

        tcb->thread_state = info->status;
    }

    if(IS_FLAG_SET(args->flags, TF_ROOT_PMAP)) {
        tcb->root_pmap = info->root_pmap;
        if(IS_ERROR(initialize_root_pmap(tcb->root_pmap))) {
            RET_MSG(ESYS_FAIL, "Unable to create initial page directory");
        }
    }

    if(IS_FLAG_SET(args->flags, TF_EXT_REG_STATE)) {
        memcpy(&tcb->xsave_state, &info->xsave_state, sizeof tcb->xsave_state);
    }

    if(IS_FLAG_SET(args->flags, TF_REG_STATE)) {
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

        thread_switch_context(tcb, false);

        // Does not return
    }

    return ESYS_OK;
}

static int handle_sys_create_int(SysCreateIntArgs* args)
{
    return irq_register(thread_get_current(), args->irq);
}

static int handle_sys_read_int(SysReadIntArgs* args)
{
    tcb_t* current_thread = thread_get_current();

    args->int_mask &= current_thread->pending_events & current_thread->event_mask;
    return E_OK;
}

static int handle_sys_update_int(SysUpdateIntArgs* args)
{
    tcb_t* current_thread = thread_get_current();

    uint32_t eoi_irqs = current_thread->pending_events & current_thread->event_mask & args->int_mask;

    /* TODO: Send EOIs to each IRQ (ignore the non-IRQ events). */


    current_thread->pending_events &= ~eoi_irqs;
    return E_OK;
}

static int handle_sys_destroy_int(SysDestroyIntArgs* args)
{
    tcb_t* current_thread = thread_get_current();

    if(current_thread == irq_handlers[args->irq])
        return irq_register(thread_get_current(), args->irq);
    else
        return E_PERM;
}

// syscall arg - syscall [lowest 8-bits]
// arg1 - ptr sender message
// arg2 - ptr to received message

static int handle_sys_send(syscall_args_t args)
{
#define TID_AND_FLAGS ((uint32_t)args.arg1)
#define SUBJECT ((uint32_t)args.arg2)
#define BUFFER ((void *)args.arg3)
#define BUFFER_LENGTH ((size_t)args.arg4)
    tcb_t* current_thread = thread_get_current();

    tid_t recipient_tid = (tid_t)(TID_AND_FLAGS & 0xFFFFu);
    uint16_t flags = (uint16_t)(TID_AND_FLAGS >> 16u);

    current_thread->user_exec_state.user_esp = args.user_stack;
    current_thread->user_exec_state.eip = args.return_address;

    switch(send_message(current_thread, recipient_tid, SUBJECT, flags, BUFFER, BUFFER_LENGTH)) {
        case E_OK:
            return ESYS_OK;
        case E_INVALID_ARG:
            return ESYS_ARG;
        case E_BLOCK:
            return ESYS_NOTREADY;
        case E_INTERRUPT:
            return ESYS_INT;
        case E_PREEMPT:
            return ESYS_PREEMPT;
        case E_FAIL:
        default:
            return ESYS_FAIL;
    }
}
#undef TID_AND_FLAGS
#undef SUBJECT
#undef BUFFER
#undef BUFFER_LENGTH

// syscall arg - syscall [lowest 8-bits]
// arg1 - ptr sender message
// arg2 - ptr to received message

static int handle_sys_receive(syscall_args_t args)
{
#define TID_AND_FLAGS ((uint32_t)args.arg1)
#define BUFFER ((void *)args.arg2)
#define BUFFER_LENGTH ((size_t)args.arg3)
    tcb_t* current_thread = thread_get_current();
    tid_t sender_tid = (tid_t)(TID_AND_FLAGS & 0xFFFFu);
    uint16_t flags = (uint16_t)(TID_AND_FLAGS >> 16u);

    current_thread->user_exec_state.user_esp = args.user_stack;
    current_thread->user_exec_state.eip = args.return_address;

    switch(receive_message(current_thread, sender_tid, flags, BUFFER, BUFFER_LENGTH)) {
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
#undef TID_AND_FLAGS
#undef BUFFER
#undef BUFFER_LENGTH

int (* const syscall_table[])(syscall_args_t) = {
    handle_sys_create,
    handle_sys_read,
    handle_sys_update,
    handle_sys_destroy,
    handle_sys_send,
    handle_sys_sleep,
    handle_sys_receive
};

/* XXX: Potential security vulnerability: User shouldn't be allowed to set arbitrary
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
        "add  $4, %esp\n"   // skip popping into eax
        "pop %ebx\n"
        "pop %ecx\n"    // Set the "user esp" to be the ecx return value
        "pop %edx\n" // edx is the return address that the user saved
        "pop %esi\n"
        "pop %edi\n"
        "pop %ebp\n"
        "popf\n"
        
        // ECX will be set as the user return ESP
        // EDX will be the return EIP and should've been set by the user
        // EBP contains the actual user ESP. The user EBP should've been saved by the user

        "sti\n" // STI must be the second to last instruction
                // to prevent interrupts from firing while in kernel mode
        "sysexit\n");
}
