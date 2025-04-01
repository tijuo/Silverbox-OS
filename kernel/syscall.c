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
    int ret_val = ESYS_FAIL;

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
                UNREACHABLE;
                break;
            default:
                UNREACHABLE;
                break;
        }
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
                UNREACHABLE;
                break;
            default:
                UNREACHABLE;
                break;
        }
    }

    if(ret_val == ESYS_OK) {
        thread_switch_context(new_thread, true);
    }

    return ret_val;
}
#undef ARG_DURATION
#undef ARG_GRANULARITY

static int handle_sys_read_page_mappings(SysReadPageMappingsArgs* args)
{
    PageMapping* mappings = args->mappings;
    size_t i;
    addr_t virt;
    paddr_t page_dir;
    pde_t pde;

    if(!mappings)
        return ESYS_FAIL;

    if(args->level >= N_PM_LEVELS) {
        return ESYS_FAIL;
    }

    if(args->addr_space == CURRENT_ROOT_PMAP) {
        args->addr_space = thread_get_current()->root_pmap;
    }

    virt = args->virt;
    page_dir = (paddr_t)ALIGN_DOWN(args->addr_space, (paddr_t)PAGE_SIZE);

    for(i = 0; i < args->count; i++, virt += (args->level == 0) ? PAGE_TABLE_SIZE : PAGE_SIZE) {
        for(int level = 0; level < args->level; level++) {
            #ifndef PAE
            if(level == 0) {
                if(IS_ERROR(read_pde(&pde, PDE_INDEX(virt), page_dir))) {
                    RET_MSG((int)i, "Unable to read PDE.");
                }

                if(!pde.is_present) {
                    mappings[i][0].flags = PM_UNMAPPED;
                    mappings[i][0].type = PMT_BLOCK;
                    mappings[i][0].block_num = pde.value >> 1;
                    break;
                } else {
                    mappings[i][0].phys_frame = get_pde_frame_number(pde);
                    mappings[i][0].flags = (!pde.is_read_write ? PM_READ_ONLY : 0) | (!pde.is_user ? PM_KERNEL : 0)
                        | (pde.pcd ? PM_UNCACHED : 0) | (pde.pwt ? PM_WRITETHRU : 0) | (pde.accessed ? PM_ACCESSED : 0);
                    
                    if(pde.is_page_sized) {
                        pmap_entry_t pmap_entry = {
                            .pde = pde
                        };

                        mappings[i][0].flags |= PM_PAGE_SIZED;
                        mappings[i][0].type = PMT_PAGE;
                        mappings[i][0].flags |= pmap_entry.large_pde.available << PM_AVAIL_OFFSET;

                        if(pmap_entry.large_pde.dirty) {
                            mappings[i][0].flags |= PM_DIRTY;
                        }
                    } else {
                        mappings[i][0].flags |= ((pde.available2 ? (1u << 4) : 0) | pde.available) << PM_AVAIL_OFFSET;
                        mappings[i][0].type = PMT_PAGE_TABLE;
                    }
                }
            } else {
                // pde is assumed to already have been loaded when level == 0

                pte_t pte;

                if(IS_ERROR(read_pte(&pte, PTE_INDEX(args->virt), PDE_BASE(pde)))) {
                    RET_MSG((int)i, "Unable to read PTE.");
                }

                if(!pte.is_present) {
                    mappings[i][1].block_num = pte.value >> 1;
                    mappings[i][1].flags = PM_UNMAPPED;
                    mappings[i][1].type = PMT_BLOCK;
                } else {
                    mappings[i][1].phys_frame = pte.base;
                    mappings[i][1].flags = PM_PAGE_SIZED;
                    mappings[i][1].type = PMT_PAGE;

                    mappings[i][1].flags |= (!pte.is_read_write ? PM_READ_ONLY : 0) | (!pte.is_user ? PM_KERNEL : 0)
                    | (pte.dirty ? PM_DIRTY : 0) | (pte.pcd ? PM_UNCACHED : 0) | (pte.pwt ? PM_WRITETHRU : 0)
                    | (pte.accessed ? PM_ACCESSED : 0) | (pte.available << PM_AVAIL_OFFSET);
                }
            }
            #else
            #error "PAE support for handle_sys_read_page_mappings() is not yet implemented."
            #endif /* PAE */
        }
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
    PageMapping* mappings = args->mappings;
    size_t i;
    addr_t virt;
    paddr_t page_dir;
    pmap_entry_t pmap_entry;

    if(!mappings)
        return ESYS_FAIL;

    if(args->level >= N_PM_LEVELS) {
        return ESYS_FAIL;
    }

    if(args->addr_space == CURRENT_ROOT_PMAP) {
        args->addr_space = thread_get_current()->root_pmap & CR3_BASE_MASK;
    }

    virt = args->virt;
    page_dir = (paddr_t)ALIGN_DOWN(args->addr_space, (paddr_t)PAGE_SIZE);

    for(i = 0; i < args->count; i++, virt += (args->level == 0) ? PAGE_TABLE_SIZE : PAGE_SIZE) {
        bool end_walk = false;
        bool was_mapped = false;

        for(int level = 0; level < args->level; level++) {
            #ifndef PAE
            unsigned int pmap_entry_index = (virt >> (12 + 10*(N_PM_LEVELS-level-1))) & (PAGE_SIZE-1);

            if(level == 0) {
                if(IS_ERROR(read_pmap_entry(page_dir, pmap_entry_index, &pmap_entry))) {
                    RET_MSG((int)i, "Unable to read PDE.");
                }

                if(pmap_entry.pde.is_present && !pmap_entry.pde.is_user) {
                    RET_MSG((int)i, "Cannot update kernel page mapping.");
                }
            } else {
                if(IS_ERROR(read_pmap_entry(PBASE_TO_PADDR(get_pde_frame_number(pmap_entry.pde)), pmap_entry_index, &pmap_entry))) {
                    RET_MSG((int)i, "Unable to read PTE.");
                }

                if(pmap_entry.pte.is_present && !pmap_entry.pte.is_user) {
                    RET_MSG((int)i, "Cannot update kernel page mapping.");
                }
            }
            #endif /* PAE */

            switch(mappings[i][level].type) {
                case PMT_BLOCK: {
                    was_mapped = !!((level == N_PM_LEVELS - 1) ? pmap_entry.pte.is_present : pmap_entry.pde.is_present);
                    pmap_entry.value = mappings[i][level].block_num << 1;
                    end_walk = true;
                    break;
                }
                case PMT_PAGE_TABLE: {
                    // XXX: Physical frames need to be checked or allocated by the kernel in
                    // order to prevent the user from mapping arbitrary address.

                    if(level == N_PM_LEVELS - 1) {
                        RET_MSG((int)i, "Page map level %d cannot be a page table.", level);
                    }

                    if(IS_FLAG_SET(mappings[i][level].flags, PM_UNMAPPED)) {
                        pmap_entry.value = 0;
                        end_walk = true;
                        break;
                    }

                    uint32_t avail_bits = ((mappings[i][level].flags & PM_AVAIL_MASK) >> PM_AVAIL_OFFSET);

                    if(avail_bits & ~0x0Fu) {
                        RET_MSG((int)i, "Cannot set available bits (overflow).");
                    }

                    pmap_entry.pde.is_read_write = !IS_FLAG_SET(mappings[i][level].flags,
                        PM_READ_ONLY);
                    pmap_entry.pde.is_user = 1;
                    pmap_entry.pde.accessed = IS_FLAG_SET(mappings[i][level].flags, PM_ACCESSED);
                    pmap_entry.pde.pcd = IS_FLAG_SET(mappings[i][level].flags, PM_UNCACHED);
                    pmap_entry.pde.pwt = IS_FLAG_SET(mappings[i][level].flags, PM_WRITETHRU);
                    pmap_entry.pde.base = PADDR_TO_PBASE(mappings[i][level].phys_frame);
                    pmap_entry.pde.available = avail_bits & 0x03u;
                    pmap_entry.pde.available2 = (avail_bits >> 2) & 0x01;
                    pmap_entry.pde.available3 = (avail_bits >> 3) & 0x01;
                    pmap_entry.pte.dirty = IS_FLAG_SET(mappings[i][level].flags, PM_DIRTY);

                    // Clear a newly mapped page table
                    if(!was_mapped && clear_phys_frames(mappings[i][level].phys_frame, 1) != 1) {
                        RET_MSG((int)i, "Unable to clear physical frame.");
                    }
                    break;
                }
                case PMT_PAGE: {
                    // XXX: Physical frames need to be checked or allocated by the kernel in
                    // order to prevent the user from mapping arbitrary address.
                    end_walk = true;
                    was_mapped = !!((level == N_PM_LEVELS - 1) ? pmap_entry.pte.is_present : pmap_entry.pde.is_present);

                    if(IS_FLAG_SET(mappings[i][level].flags, PM_UNMAPPED)) {
                        pmap_entry.value = 0;
                        end_walk = true;
                        break;
                    }

                    if(level == N_PM_LEVELS - 1) {
                        uint32_t avail_bits = ((mappings[i][level].flags & PM_AVAIL_MASK) >> PM_AVAIL_OFFSET);

                        if(avail_bits & ~0x03u) {
                            RET_MSG((int)i, "Cannot set available bits (overflow).");
                        }

                        pmap_entry.pte.is_read_write = !IS_FLAG_SET(mappings[i][level].flags, PM_READ_ONLY);
                        pmap_entry.pte.is_user = 1;
                        pmap_entry.pte.accessed = IS_FLAG_SET(mappings[i][level].flags, PM_ACCESSED);
                        pmap_entry.pte.pcd = IS_FLAG_SET(mappings[i][level].flags, PM_UNCACHED);
                        pmap_entry.pte.pwt = IS_FLAG_SET(mappings[i][level].flags, PM_WRITETHRU);
                        pmap_entry.pte.base = PADDR_TO_PBASE(mappings[i][level].phys_frame);
                        pmap_entry.pte.available = avail_bits & 0x03u;
                        pmap_entry.pte.dirty = IS_FLAG_SET(mappings[i][level].flags, PM_DIRTY);
                    } else {
                        uint32_t avail_bits = ((mappings[i][level].flags & PM_AVAIL_MASK) >> PM_AVAIL_OFFSET);

                        if(avail_bits & ~0x03u) {
                            RET_MSG((int)i, "Cannot set available bits (overflow).");
                        }

                        pmap_entry.large_pde.is_read_write = !IS_FLAG_SET(mappings[i][level].flags,
                            PM_READ_ONLY);
                        pmap_entry.large_pde.is_user = 1;
                        pmap_entry.large_pde.accessed = IS_FLAG_SET(mappings[i][level].flags, PM_ACCESSED);
                        pmap_entry.large_pde.pcd = IS_FLAG_SET(mappings[i][level].flags, PM_UNCACHED);
                        pmap_entry.large_pde.pwt = IS_FLAG_SET(mappings[i][level].flags, PM_WRITETHRU);

                        pmap_entry.large_pde.is_page_sized = 1;
                        pmap_entry.large_pde.dirty = IS_FLAG_SET(mappings[i][level].flags, PM_DIRTY);
                        pmap_entry.large_pde.global = IS_FLAG_SET(mappings[i][level].flags, PM_STICKY);
                        pmap_entry.large_pde.pat = IS_FLAG_SET(mappings[i][level].flags, PM_WRITECOMB)
                            && !IS_FLAG_SET(mappings[i][level].flags, PM_UNCACHED);
                        pmap_entry.large_pde.available = avail_bits & 0x03;
                        pmap_entry.large_pde.base_lower = ((mappings[i][level].phys_frame)
                            >> 10) & 0x3FFu;
                        pmap_entry.large_pde.base_upper = (mappings[i][level].phys_frame) >> 20u;
                        pmap_entry.large_pde._resd = 0;
                        end_walk = true;
                    }
                    break;
                }
                default:
                    RET_MSG((int)i, "Unrecognized mapping type: %d (mapping %i, level %i).", mappings[i][level].type, i, level);
            }

            if(was_mapped && page_dir == args->addr_space) {
                invalidate_page(virt);
            }

            #ifndef PAE
            if(level == 0) {
                if(IS_ERROR(write_pmap_entry(args->addr_space, pmap_entry_index, pmap_entry))) {
                    RET_MSG((int)i, "Unable to write to PDE.");
                }
            } else {
                if(IS_ERROR(write_pmap_entry(PBASE_TO_PADDR(get_pde_frame_number(pmap_entry.pde)), pmap_entry_index, pmap_entry))) {
                    RET_MSG((int)i, "Unable to write to PDE.");
                }
            }
            #endif /* PAE */

            if(end_walk) {
                break;
            }

            #ifdef PAE
            #error "PAE support for handle_sys_update_page_mappings() is not yet implemented."
            #endif /* PAE */
        }
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
    } else {
        RET_MSG(
            ESYS_PERM,
            "Calling thread doesn't have permission to execute this system call.");
    }
}

// arg1 - tid

static int handle_sys_destroy_tcb(SysDestroyTcbArgs* args)
{
    if(1) {
        tcb_t* tcb = get_tcb(args->tid);
        return tcb && !IS_ERROR(thread_release(tcb)) ? ESYS_OK : ESYS_FAIL;
    } else {
        RET_MSG(
            ESYS_PERM,
            "Calling thread doesn't have permission to execute this system call.");
    }
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

            info->fxsave_state = (void*)tcb->fxsave_state;
            info->xsave_rfbm = tcb->xsave_rfbm;
            info->fxsave_state_len = tcb->fxsave_state_len;

            return ESYS_OK;
        } else {
            RET_MSG(
                ESYS_PERM,
                "Calling thread doesn't have permission to execute this system call.");
        }
        return ESYS_FAIL;
    }
}
// arg1 - tid
// arg2 - flags
// arg3 - info

static int handle_sys_update_tcb(SysUpdateTcbArgs* args)
{
    if(0) {
        RET_MSG(
            ESYS_PERM,
            "Calling thread doesn't have permission to execute this system call.");
    }

    thread_info_t* info = args->info;
    tcb_t* tcb = args->tid == NULL_TID ? thread_get_current() : get_tcb(args->tid);

    if(!tcb || tcb->thread_state == INACTIVE) {
        RET_MSG(ESYS_ARG, "The specified thread (tid: %hhu) doesn't exist.", args->tid);
    }

    if(IS_FLAG_SET(args->flags, TF_STATUS)) {

        switch(info->status) {
            case READY:
            case PAUSED:
                break;
            default:
                RET_MSG(ESYS_ARG, "Unable to change thread status to %d.", info->status);
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
        memcpy(&tcb->fxsave_state, &info->fxsave_state, sizeof tcb->fxsave_state);
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
