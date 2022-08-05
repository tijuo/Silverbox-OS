#include <kernel/thread.h>
#include <kernel/multiboot2.h>
#include <kernel/mm.h>
#include <kernel/thread.h>
#include <elf.h>
#include "init.h"
#include "memory.h"
#include <kernel/memory.h>

#define INIT_SERVER_STACK_TOP   0x00ul
#define INIT_SERVER_FLAG  "--init"

DISC_CODE int map_page(addr_t addr, paddr_t phys, elf64_pheader_t *pheader,
                       paddr_t addr_space);
NON_NULL_PARAMS DISC_CODE int load_init_server(
    struct multiboot_info_header *header);
DISC_CODE static tcb_t *load_elf_exe(addr_t, paddr_t, void *);
NON_NULL_PARAMS DISC_CODE static bool is_valid_elf_exe(elf64_header_t *image);
DISC_CODE static void bootstrap_init_server(void);
DISC_DATA static addr_t init_server_img;

extern tcb_t *init_server_thread;
extern addr_t first_free_page;

/**
 * Locates and loads the initial server that was passed in to GRUB via the module2 command.
 */
int load_init_server(struct multiboot_info_header *header) {
    bool init_server_found = false;

    const struct multiboot_tag *tags = header->tags;

    while(tags->type != MULTIBOOT_TAG_TYPE_END) {
        switch(tags->type) {
            case MULTIBOOT_TAG_TYPE_MODULE: {
                    const struct multiboot_tag_module *module =
                        (const struct multiboot_tag_module *)tags;
                    if(!init_server_img)
                        init_server_img = (addr_t)module->mod_start;

                    const char *str_ptr = (const char *)module->cmdline;

                    do {
                        str_ptr = (const char *)strstr(str_ptr, INIT_SERVER_FLAG);
                        const char *separator = str_ptr + strlen(INIT_SERVER_FLAG);

                        if(*separator == '\0' || (*separator >= '\t' && *separator <= '\r')) {
                            init_server_found = true;
                            init_server_img = (addr_t)module->mod_start;
                            break;
                        } else
                            str_ptr = separator;
                    } while(str_ptr);

                    break;
                }
            default:
                break;
        }

        tags = (const struct multiboot_tag *)((uintptr_t)tags + tags->size
                                              + ((tags->size % 8) == 0 ? 0 : 8 - (tags->size % 8)));
    }

    if(!init_server_found) {
        if(init_server_img)
            kprintf(
                "Initial server was not specified with --init. Using first module.\n");
        else {
            kprintf("Can't find initial server.");
            return E_FAIL;
        }
    }

    bootstrap_init_server();

    return E_OK;
}

/**
 * Determines whether an ELF image is a valid ELF64 executable.
 *
 * @return true, if valid. false, otherwise
 */
bool is_valid_elf_exe(elf64_header_t *image) {
    return VALID_ELF(image) && image->identifier[EI_VERSION] >= EV_CURRENT
           && image->identifier[EI_DATA] == ELFDATA2LSB
           && image->identifier[EI_OSABI] == ELFOSABI_SYSV
           && image->identifier[EI_ABIVERSION] == 0 && image->type == ET_EXEC
           && image->machine == EM_X86_64 && image->version >= EV_CURRENT
           && image->identifier[EI_CLASS] == ELFCLASS64;
}

/**
 * Maps a physical frame to a virtual page.
 * @param addr The virtual address.
 * @param phys The physical frame.
 * @param pheader An ELF program header if mapping to a segment (for setting paging access bits). NULL, otherwise.
 * @param addr_space The address space in which to map.
 */

int map_page(addr_t addr, paddr_t phys, elf64_pheader_t *pheader,
             paddr_t addr_space) {
    volatile pmap_entry_t *pmap_entry;
    pmap_entry_t entry;
    int level;

    level = get_pmap_entry(0, addr, &pmap_entry, addr_space);

    if(level == E_OK && IS_FLAG_SET(*pmap_entry, PAE_PRESENT)) {
        kprintf("Address: %#lx is already mapped in address space: %#lx\n", addr,
                addr_space);
        RET_MSG(
            E_FAIL,
            "Error: Memory is already mapped where segment is supposed to be loaded.");
    }

    while(level < -1) {
        paddr_t new_table = alloc_page_frame();

        clear_phys_page(new_table);
        entry = PAE_US | PAE_RW | PAE_PRESENT;
        set_pmap_entry_base(-level - 1, &entry, new_table);

        *pmap_entry = entry;
        level = get_pmap_entry(0, addr, &pmap_entry, addr_space);
    }

    entry = PAE_PRESENT;

    set_pmap_entry_base(0, &entry, phys);

    if(pheader) {
        if(IS_FLAG_SET(pheader->flags, PF_R))
            entry |= PAE_US;

        if(IS_FLAG_SET(pheader->flags, PF_W))
            entry |= PAE_RW;

        if(!IS_FLAG_SET(pheader->flags, PF_X))
            entry |= PAE_XD;
    } else {
        entry |= PAE_US | PAE_RW;
    }

    *pmap_entry = entry;

    return E_OK;
}

/**
 * Creates a new thread and loads an ELF executable.
 * @param img The ELF64 executable image.
 * @param addr_space The address space for the thread.
 * @param user_stack The top of the thread's user stack.
 */

tcb_t *load_elf_exe(addr_t img, paddr_t addr_space, void *user_stack) {
    elf64_header_t *image = (elf64_header_t *)img;

    if(!is_valid_elf_exe(image)) {
        kprintf("Not a valid ELF64 executable.\n");
        return NULL;
    }

    tcb_t *thread = create_thread((void *)image->entry, addr_space, user_stack);

    if(thread == NULL) {
        kprintf("load_elf_exe(): Couldn't create thread.\n");
        return NULL;
    }

    for(size_t i = 0; i < image->phnum; i++) {
        elf64_pheader_t *pheader = (elf64_pheader_t *)(img + image->phoff
                                   + i * image->phentsize);

        if(pheader->type == PT_LOAD) {
            // Map the file image
            for(addr_t addr = pheader->vaddr; addr < pheader->vaddr + pheader->filesz;
                    addr += PAGE_SIZE) {
                if(IS_ERROR(
                            map_page(addr, pheader->offset + img + (addr - pheader->vaddr),
                                     pheader, addr_space)))
                    RET_MSG(NULL, "Unable to map page");
            }

            if(pheader->memsz > pheader->filesz) {
                // Then map the remainder of the memory image (filled with zeros)

                addr_t file_img_end = pheader->vaddr + pheader->filesz;
                addr_t boundary = ALIGN(pheader->vaddr + pheader->filesz, PAGE_SIZE);

                paddr_t paddr;

                if(IS_ERROR(translate_vaddr(file_img_end, &paddr, addr_space)))
                    RET_MSG(NULL, "Unable to clear memory.");

                if(file_img_end != boundary)
                    memset((void *)paddr, 0, boundary - file_img_end);

                for(addr_t addr = boundary; addr < pheader->vaddr + pheader->memsz;
                        addr += PAGE_SIZE) {
                    paddr_t blank_page = alloc_page_frame();
                    clear_phys_page(blank_page);

                    if(IS_ERROR(map_page(addr, blank_page, NULL, addr_space)))
                        RET_MSG(NULL, "Unable to map blank page");
                }
            }
        } else {
            kprintf("Found program header type: %u. Skipping...", pheader->type);
        }
    }

    return thread;
}

struct InitStackArgs {
    uint64_t return_address;
    addr_t first_free_page;
    size_t stack_size;
    unsigned char code[8];
};

/**
 Bootstraps the initial server and passes necessary boot data to it.
 */

void bootstrap_init_server(void) {
    addr_t init_server_stack = (addr_t)INIT_SERVER_STACK_TOP;
    elf64_header_t *elf_header = (elf64_header_t *)init_server_img;

    /* code:

     nop x 6
     ud2             # Trigger Invalid Opcode Exception: #UD
     */

    struct InitStackArgs stack_data = {
        .return_address = init_server_stack
        - sizeof((struct InitStackArgs *)0)->code,
        .first_free_page = first_free_page,
        .stack_size = 0,
        .code = {
            0x90,
            0x90,
            0x90,
            0x90,
            0x90,
            0x90,
            0x0F,
            0x0B
        }
    };

    kprintf("Bootstrapping initial server...\n");

    if(!is_valid_elf_exe(elf_header)) {
        kprintf("Invalid ELF executable\n");
        goto failed_bootstrap;
    }

    paddr_t addr_space = alloc_page_frame();

    if(addr_space == INVALID_PFRAME) {
        kprintf("Unable to root page map for initial server.\n");
        goto failed_bootstrap;
    }

    clear_phys_page(addr_space);

    for(size_t p = 0; p < (512 * 1024) / PAGE_SIZE; p++) {
        addr_t stack_page = alloc_page_frame();

        if(stack_page == INVALID_PFRAME) {
            kprintf("Unable to initialize stack page.\n");
            goto failed_bootstrap;
        }

        if(IS_ERROR(
                    map_page(init_server_stack-(p+1)*PAGE_SIZE, stack_page, NULL, addr_space))) {
            kprintf("Unable to map stack page.");
            goto failed_bootstrap;
        };

        // Copy bootstrap args to stack for the initial server to use

        if(p == 0)
            memcpy((void *)(stack_page + PAGE_SIZE - sizeof(stack_data)), &stack_data,
                   sizeof(stack_data));

        stack_data.stack_size += PAGE_SIZE;
    }

    if((init_server_thread = load_elf_exe(
                                 init_server_img, addr_space,
                                 (void *)(init_server_stack - sizeof(stack_data)))) == NULL) {
        kprintf("Unable to load ELF executable.\n");
        goto failed_bootstrap;
    }

    kprintf("Starting initial server... %#p\n", init_server_thread);

    if(IS_ERROR(start_thread(init_server_thread)))
        goto release_init_thread;

    return;

release_init_thread:
    release_thread(init_server_thread);

failed_bootstrap:
    kprintf("Unable to start initial server.\n");
    return;
}

