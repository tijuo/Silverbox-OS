#include <os/multiboot.h>
#include <os/syscalls.h>
#include <oslib.h>
#include <types.h>
#include <string.h>
#include <stdlib.h>
#include "phys_mem.h"
#include "addr_space.h"
#include "pager.h"
#include <os/elf.h>
#include <os/services.h>
#include <os/msg/message.h>
#include <os/msg/init.h>
#include <os/msg/kernel.h>
#include "name/name.h"
#include "elf.h"
#include <stdio.h>

#define STACK_TOP		    0xC0000000
#define MESSAGE_BUF_LEN     256

extern StringHashTable thread_names;
extern StringHashTable fs_names;
extern StringHashTable fs_table;

extern int driver_init(void);
extern void driver_cleanup(void);

IntHashTable server_table; // id -> ServerEntry
int zero_dev_fd = -1;

int init_page_stack(multiboot_info_t* info, addr_t first_free_page);
static int load_elf_exe(int exe_fd, module_t* module);
static void handle_message(msg_t* msg, size_t bytes_received);
tid_t thread_create_new(tid_t desired_tid, void* entry, paddr_t page_map, void* stack_top);

extern void (*idle)(void);
int thread_start(tid_t tid);
int thread_set_priority(tid_t tid, int priority);

struct ServerEntry {
    tid_t tid;
    int type;
};

int init_page_stack(multiboot_info_t* info, addr_t first_free_page)
{
    memory_map_t mmap;
    size_t free_page_count = 0;
    multiboot_info_t multiboot_struct;

    free_page_stack_top = free_page_stack;

    // Read the multiboot info structure from physical memory

    if(peek((addr_t)info, &multiboot_struct, sizeof(multiboot_struct)) != 0) {
        fprintf(stderr, "Failed to read multiboot struct.\n");
        return 1;
    }

    /* Count the total number of pages and the pages we'll need to map the stack.
       Then map the stack with those pages. Finally, add the remaining pages to
       the free stack. */

    for(int pass = 0; pass < 2; pass++) {
        addr_t free_page = first_free_page + PAGE_SIZE;
        size_t pages_needed;
        size_t pages_left;
        addr_t ptr;

        if(pass == 1) {
            pages_needed = (free_page_count * sizeof(addr_t)) / PAGE_SIZE;

            if((free_page_count * sizeof(addr_t)) & (PAGE_SIZE - 1))
                pages_needed++;

            pages_left = free_page_count - pages_needed;
            ptr = (addr_t)free_page_stack;
        }

        for(unsigned int offset = 0; offset < multiboot_struct.mmap_length; offset += mmap.size + sizeof(mmap.size)) {
            if(peek((addr_t)(multiboot_struct.mmap_addr + offset), &mmap, sizeof(memory_map_t)) != 0) {
                fprintf(stderr, "Failed to read memory map");
                return 1;
            }

            if(mmap.type == MBI_TYPE_AVAIL) {
                u64 mmap_len = mmap.length_high;
                mmap_len = mmap_len << 32;
                mmap_len |= mmap.length_low;

                u64 mmap_base = mmap.base_addr_high;
                mmap_base = mmap_base << 32;
                mmap_base |= mmap.base_addr_low;

                if(pass == 0)
                    for(; free_page >= mmap_base && free_page < mmap_base + mmap_len; free_page += PAGE_SIZE, free_page_count++);
                else {
                    pbase_t free_frame = (pbase_t)(free_page >> 12);

                    if(sys_map(NULL, (void*)ptr, &free_frame, pages_needed, PM_READ_WRITE) != (int)pages_needed) {
                        fprintf(stderr, "Unable to allocate memory for free page stack.\n");
                        return 1;
                    }

                    free_page += PAGE_SIZE * pages_needed;
                    ptr += PAGE_SIZE * pages_needed;
                    pages_needed = 0;

                    for(; pages_left; pages_left--, free_page += PAGE_SIZE)
                        *free_page_stack_top++ = free_page;
                }
            }
        }
    }

    return 0;
}

static void handle_message(msg_t* msg, size_t bytes_received)
{
    if(msg->target.sender == KERNEL_TID) {
        switch(msg->subject) {
            case EXCEPTION_MSG:
            {
                struct ExceptionMessage* ex_message = (struct ExceptionMessage*)&msg->buffer;
                addr_t fault_addr = ex_message->esp;
                tid_t tid = ex_message->who;
                int int_num = ex_message->fault_num;
                int error_code = ex_message->error_code;

                if(int_num == 14) {
                    /*

                              fprintf(stderr, " Fault address: %#x", fault_addr);
                    */
                    if(!(error_code & 0x4)) {
                        fprintf(stderr, " Illegal access to supervisor memory.\n");
                        break;
                    }

                    AddrSpace* aspace = lookup_tid(tid);

                    // XXX: Read/writes for devices need to be converted to physical pages
                    // somehow. The pager could create a shared memory region between the 
                    // device driver and use it to map pages

                    if(!aspace) {
                        fprintf(stderr, "Unable to find address space for TID %hhu\n", tid);
                    } else if(find_address(aspace, fault_addr)) {
                        MemoryMapping* mapping = get_mapping(aspace, fault_addr);
                        PageMapping page_mappings[1];
                        SysReadPageMappingsArgs mapping_args = {
                            .addr_space = aspace->phys_addr,
                            .count = 1,
                            .level = 0,
                            .mappings = &page_mappings[0],
                            .virt = fault_addr
                        };

                        if(!mapping) {
                            fprintf(stderr, "Unable to find memory region for fault address.\n");
                            break;
                        }

                        if(sys_read(RES_PAGE_MAPPING, &mapping_args) != 0) {
                            fprintf(stderr, "Unable to read page mapping.\n");
                        }

                        if(page_mappings[0][0].flags & PM_UNMAPPED) {
                            page_mappings[0][0].phys_frame = (pbase_t)alloc_phys_frame() >> 12;
                        }

                        // XXX: Check error code to determine what to do
                       /* Allowed page faults:

                          Access non-present memory, that exists in mapping
                          Write to read-only memory marked as COW

                          Not allowed:

                          Access supervisor memory

                          If region is marked as ZERO, then clear page
                          If region is marked as COW and a write was performed, create new page, map it as r/w, and copy data
                       */

                        page_mappings[0][0].flags = mapping->flags & REG_RO ? PM_READ_ONLY : 0;

                        if(sys_update(RES_PAGE_MAPPING, &mapping_args) != 0) {
                            fprintf(stderr, "Unable to write page mapping.\n");
                        }

                        // Extend the stack guard page, if possible.

                        if(region->flags & REG_GUARD) {
                            region->flags &= ~REG_GUARD;

                            // XXX: This should only be done if the next guard page is unmapped
                            // XXX: otherwise terminate the thread

                            region_map(aspace, (fault_addr & ~(PAGE_SIZE - 1))
                                + ((region->flags & REG_DOWN) ? -PAGE_SIZE : PAGE_SIZE),
                                CURRENT_ROOT_PMAP, 1, REG_GUARD | (region->flags & REG_DOWN));
                        }
                        thread_start(tid);
                    } else {
                        fprintf(stderr, "Fault address not found in mapping.\n");
                    }
                } else {
                    fprintf(stderr, "Exception %d for TID %hhu.", int_num, tid, error_code);
                }
            }
            //        fprintf(stderr, "\n");
            break;

            case EXIT_MSG:
            {
                struct ExitMessage* exit_msg = (struct ExitMessage*)&msg->buffer;
                fprintf(stderr, "TID %hhu exited with status code: %d\n", exit_msg->who, exit_msg->status_code);
                break;
            }
            default:
            {
                fprintf(stderr, "Unhandled message with subject: %d\n", msg->subject);
                break;
            }
        }
    } else {
        msg_t response_msg;

        switch(msg->subject) {
            case MAP_MEM:
                break;
            case UNMAP_MEM:
                break;
            case CREATE_PORT:
                break;
            case DESTROY_PORT:
                break;
            case SEND_MESSAGE:
                break;
            case RECEIVE_MESSAGE:
                break;
            case REGISTER_SERVER:
            {
                struct ServerEntry* entry;
                struct RegisterServerRequest* request = (struct RegisterServerRequest*)&msg->buffer;

                response_msg.target.recipient = msg->target.sender;
                response_msg.flags = MSG_STD;
                response_msg.buffer = NULL;
                response_msg.buffer_length = 0;

                if(int_hash_table_lookup(&server_table, msg->target.sender, NULL, NULL) != 0) {
                    response_msg.subject = RESPONSE_FAIL;
                    break;
                }

                entry = malloc(sizeof(struct ServerEntry));

                if(!entry) {
                    response_msg.subject = RESPONSE_ERROR;
                    break;
                }

                entry->tid = msg->target.sender;
                entry->type = request->type;

                if(int_hash_table_insert(&server_table, msg->target.sender, entry, sizeof * entry) != 0) {
                    free(entry);
                    response_msg.subject = RESPONSE_ERROR;
                    break;
                }

                response_msg.subject = RESPONSE_OK;
                break;
            }
            case UNREGISTER_SERVER:
            {
                struct ServerEntry* entry;

                if(int_hash_table_remove_value(&server_table, msg->target.sender, (void*)&entry, NULL) != 0) {
                    response_msg.subject = RESPONSE_FAIL;
                } else {
                    free(entry);
                    response_msg.subject = RESPONSE_OK;
                }
                break;
            }
            case REGISTER_NAME:
                name_register(msg, &response_msg);
                break;
            case LOOKUP_NAME:
                name_lookup(msg, &response_msg);
                break;
            case UNREGISTER_NAME:
                name_unregister(msg, &response_msg);
                break;
            default:
                response_msg.subject = RESPONSE_FAIL;
                break;
        }

        response_msg.target.recipient = msg->target.sender;

        sys_send(&response_msg, NULL);
    }
}

tid_t thread_create_new(tid_t desired_tid, void* entry, paddr_t page_map, void* stack_top)
{
    AddrSpace* addr_space = lookup_page_map(page_map);
    SysCreateTcbArgs args = { .entry = entry, .addr_space = page_map, .stack_top = stack_top, .tid = desired_tid };

    if(sys_create(RES_TCB, &args) == ESYS_OK) {
        if(attach_tid(addr_space, args.tid) == 0) {
            return args.tid;
        }
    }
    return NULL_TID;
}

static int load_elf_exe(int exe_fd, module_t* module)
{
    unsigned phtab_count;
    elf_header_t* image = NULL;
    elf_pheader_t* pheader;
    tid_t tid;
    int fail = 0;
    void* stack_top = (void*)STACK_TOP;
    size_t module_size = module->mod_end - module->mod_start;

    if(!module)
        return -1;

    image = malloc(module_size);

    if(!image)
        return -1;

    if(peek(module->mod_start, (void*)image, module_size) != ESYS_OK) {
        if(image)
            free(image);

        return -1;
    }

    if(!is_valid_elf_exe(image)) {
        fprintf(stderr, "Not a valid ELF executable.\n");

        if(image)
            free(image);

        return -1;
    }

    paddr_t pmap = (paddr_t)alloc_phys_frame();
    AddrSpace* addr_space = malloc(sizeof(AddrSpace));

    if(!pmap || !addr_space) {
        fail = 1;
    } else {
        addr_space_init(addr_space, pmap);
        addr_space_add(addr_space);

        phtab_count = image->phnum;
        pheader = (elf_pheader_t*)((unsigned)image + image->phoff);

        for(unsigned int i = 0; i < phtab_count; i++, pheader++) {
            if(fail) {
                break;
            }
            
            if(pheader->type == PT_LOAD) {
                if(pheader->filesz > 0) {
                    MemoryMapping mapping = {
                        .fd = exe_fd,
                        .file_offset = pheader->offset,
                        .permissions = 0,
                        .flags =((pheader->flags & PF_W) ? 0 : REG_RO) | ((pheader->flags & PF_X) ? 0 : REG_NOEXEC),
                        .memory_region = {
                            .start = pheader->vaddr,
                            .end = pheader->vaddr + pheader->filesz
                        }
                    };
                    
                    if(attach_addr_region(addr_space, &mapping) != 0) {
                        fprintf(stderr, "Failed to add memory region %#x:%#x to address space.", pheader->vaddr, pheader->memsz);
                        fail = 1;
                        break;
                    }
                }

                if(pheader->memsz > pheader->filesz) {
                    MemoryMapping mapping = {
                        .fd = zero_dev_fd,
                        .file_offset = 0,
                        .permissions = 0,
                        .flags =((pheader->flags & PF_W) ? 0 : REG_RO) | ((pheader->flags & PF_X) ? 0 : REG_NOEXEC),
                        .memory_region = {
                            .start = pheader->vaddr + pheader->filesz,
                            .end = pheader->vaddr + pheader->memsz
                        }
                    };
                    
                    if(attach_addr_region(addr_space, &mapping) != 0) {
                        fprintf(stderr, "Failed to add memory region %#x:%#x to address space.", pheader->vaddr, pheader->memsz);
                        fail = 1;
                        break;
                    }
                }
            }
        }
    }

    if(!fail) {
        tid = thread_create_new(NULL_TID, (void*)image->entry, pmap, stack_top);

        if(tid == NULL_TID) {
            fail = 1;
        } else {
            MemoryMapping mapping = {
                .fd = zero_dev_fd,
                .file_offset = 0,
                .permissions = 0,
                .flags = REG_DOWN | REG_GUARD,
                .memory_region = {
                    .start = STACK_TOP - PAGE_SIZE,
                    .end = STACK_TOP
                }
            };

            if(attach_addr_region(addr_space, &mapping) != 0) {
                fail = 1;
            } else if(thread_start(tid) != 0) {
                fail = 1;
            }
        }
    }

    if(fail) {
        fprintf(stderr, "Failure in load_elf_exec\n");

        if(tid != NULL_TID && addr_space) {
            detach_tid(addr_space, tid);
        }

        if(addr_space) {
            addr_space_destroy(addr_space);
            free(addr_space);
        }

        if(pmap) {
            free_phys_frame((addr_t)pmap);
        }

        if(image) {
            free(image);
        }

        return -1;
    }

    if(image) {
        free(image);
    }

    return 0;
}

int thread_start(tid_t tid)
{
    thread_info_t thread_info = { .status = TCB_STATUS_READY };
    SysUpdateTcbArgs args = {
        .flags = TF_STATUS,
        .tid = tid,
        .info = &thread_info
    };

    return sys_update(RES_TCB, &args) == ESYS_OK ? 0 : -1;
}

int thread_set_priority(tid_t tid, int priority)
{
    thread_info_t thread_info = { .priority = priority };
    SysUpdateTcbArgs args = {
        .flags = TF_PRIORITY,
        .tid = tid,
        .info = &thread_info
    };

    return sys_update(RES_TCB, &args) == ESYS_OK ? 0 : -1;
}

int main(multiboot_info_t* info, addr_t first_free_page)
{
    msg_t msg;
    char buffer[MESSAGE_BUF_LEN];
    int exe_fd = -1;

    thread_info_t thread_info;
    SysReadTcbArgs tcb_args = { .tid = NULL_TID, .info = &thread_info, .info_length = sizeof thread_info };

    fprintf(stderr, "Init server started.\n");

    if(init_page_stack(info, first_free_page) != 0) {
        fprintf(stderr, "Unable to initialize the free page stack.\n");
    }

    if(sys_read(RES_TCB, &tcb_args) != ESYS_OK) {
        fprintf(stderr, "Unable to read TCB info.\n");
        return EXIT_FAILURE;
    }

    msg.buffer = buffer;
    msg.buffer_length = MESSAGE_BUF_LEN;

    driver_init();

    string_hash_table_init(&addr_spaces, 127, &GLOBAL_ALLOCATOR);
    string_hash_table_init(&thread_names, 127, &GLOBAL_ALLOCATOR);
    string_hash_table_init(&fs_names, 127, &GLOBAL_ALLOCATOR);
    string_hash_table_init(&fs_table, 127, &GLOBAL_ALLOCATOR);
    int_hash_table_init(&server_table, 127, &GLOBAL_ALLOCATOR);

    addr_space_init(&initsrv_addr_space, tcb_args.info->root_pmap);
    addr_space_add(&initsrv_addr_space);

    tid_t idle_tid = thread_create_new(NULL_TID, &idle, CURRENT_ROOT_PMAP, NULL);

    if(idle_tid == NULL_TID || thread_set_priority(idle_tid, 0) != 0 || thread_start(idle_tid) != 0) {
        fprintf(stderr, "Unable to start idle thread\n");
        return 1;
    }

    multiboot_info_t multiboot_struct;
    module_t module;

    peek((addr_t)info, &multiboot_struct, sizeof multiboot_struct);

    // XXX: assume the first module is the initial server (and skip it)

    for(unsigned i = 1; i < multiboot_struct.mods_count; i++) {
        peek((addr_t)multiboot_struct.mods_addr + i * sizeof(module_t), &module, sizeof(module_t));

        if(load_elf_exe(exe_fd, &module) != 0) {
            fprintf(stderr, "Unable to load elf module %d\n", i);
        }
    }

    while(1) {
        msg.target.sender = ANY_SENDER;
        int code;
        size_t bytes_received;

        if((code = sys_receive(&msg, &bytes_received)) == ESYS_OK) {
            handle_message(&msg, bytes_received);
        } else {
            fprintf(stderr, "sys_receive() failed with code: %d\n");
            return EXIT_FAILURE;
        }
    }

    string_hash_table_destroy(&addr_spaces);
    string_hash_table_destroy(&thread_names);
    string_hash_table_destroy(&fs_names);
    string_hash_table_destroy(&fs_table);
    int_hash_table_destroy(&server_table);

    driver_cleanup();

    return EXIT_FAILURE;
}