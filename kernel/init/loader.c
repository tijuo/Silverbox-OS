#include <kernel/thread.h>
#include <kernel/multiboot.h>
#include <kernel/mm.h>
#include <kernel/thread.h>
#include <elf.h>
#include "init.h"
#include "memory.h"
#include <kernel/memory.h>

#define INIT_SERVER_FLAG  "--init"

DISC_CODE int load_servers(multiboot_info_t* info);
DISC_CODE static tcb_t* load_elf_exe(addr_t, uint32_t, void*);
DISC_CODE static bool is_valid_elf_exe(elf_header_t* image);
DISC_CODE static int bootstrap_init_server(multiboot_info_t* info);
DISC_DATA static addr_t init_server_img;

extern tcb_t* init_server_thread;
extern addr_t first_free_page;

extern bool can_alloc_frames;

int load_servers(multiboot_info_t* info)
{
    module_t* module;
    bool init_server_found = false;

    kprintfln("Boot info struct: %#p\nModules located at %#p. %d modules found.",
        (void*)VIRT_TO_PHYS((addr_t)info), info->mods_addr, info->mods_count);

    module = (module_t*)PHYS_TO_VIRT(info->mods_addr);

    /* Locate the initial server module. */

    for(size_t i = info->mods_count; module && i; i--, module++) {
        if(i == info->mods_count)
            init_server_img = (addr_t)module->mod_start;

        if(module->string) {
            const char* str_ptr = (const char*)PHYS_TO_VIRT(module->string);

            do {
                str_ptr = (const char*)strstr(str_ptr, INIT_SERVER_FLAG);
                const char* separator = str_ptr + strlen(INIT_SERVER_FLAG);

                if(*separator == '\0' || (*separator >= '\t' && *separator <= '\r')) {
                    init_server_found = true;
                    init_server_img = (addr_t)module->mod_start;
                    break;
                } else
                    str_ptr = separator;
            } while(str_ptr);
        }
    }

    if(!init_server_found) {
        if(info->mods_count)
            kprintfln(
                "Initial server was not specified with --init. Using first module.");
        else {
            kprintfln("Can't find initial server.");
            return E_FAIL;
        }
    }

    bootstrap_init_server(info);

    return E_OK;
}

bool is_valid_elf_exe(elf_header_t* image)
{
    return image && VALID_ELF(image)
        && image->identifier[EI_VERSION] == EV_CURRENT
        && image->type == ET_EXEC && image->machine == EM_386
        && image->version == EV_CURRENT
        && image->identifier[EI_CLASS] == ELFCLASS32;
}

tcb_t* load_elf_exe(addr_t img, addr_t addr_space, void* user_stack)
{
    elf_header_t image;
    elf_sheader_t sheader;
    tcb_t* thread;
    addr_t page;
    pde_t pde;
    pte_t pte;
    size_t i;
    size_t offset;

    if(IS_ERROR(peek(img, &image, sizeof image))) {
        RET_MSG(NULL, "Unable to read ELF header.");
    }

    pte.is_present = 0;

    if(!is_valid_elf_exe(&image)) {
        RET_MSG(NULL, "Not a valid ELF executable.");
    }

    thread = thread_create((void*)image.entry, addr_space, user_stack);

    if(thread == NULL) {
        RET_MSG(NULL, "load_elf_exe(): Couldn't create thread.");
    }

    /* Create the page table before mapping memory */

    for(i = 0; i < image.shnum; i++) {
        if(IS_ERROR(peek((img + image.shoff + i * image.shentsize), &sheader, sizeof sheader))) {
            RET_MSG(NULL, "Unable to read ELF section header.");
        }

        if(!IS_FLAG_SET(sheader.flags, SHF_ALLOC)) {
            continue;
        }

        for(offset = 0; offset < sheader.size; offset += PAGE_SIZE) {
            if(IS_ERROR(read_pde(&pde, PDE_INDEX(sheader.addr + offset), addr_space))) {
                RET_MSG(NULL, "Unable to read PDE");
            }

            if(!pde.is_present) {
                page = alloc_phys_frame();

                if(page == INVALID_PADDR) {
                    RET_MSG(NULL, "load_elf_exe(): Unable to allocate PDE.");
                }

                if(clear_phys_frames(page, 1) != 1) {
                    kprintf("Unable to clear physical frame");
                }

                pde.base = (pbase_t)ADDR_TO_PBASE(page);
                pde.is_read_write = 1;
                pde.is_user = 1;
                pde.is_present = 1;
                pde.was_allocated = 1;

                if(IS_ERROR(write_pde(PDE_INDEX(sheader.addr + offset), pde, addr_space))) {
                    RET_MSG(NULL, "load_elf_exe(): Unable to write PDE.");
                }
            } else if(pde.is_page_sized) {
                RET_MSG(NULL, "load_elf_exe(): Memory region has already been mapped to a large page.");
            }

            if(IS_ERROR(read_pte(&pte, PTE_INDEX(sheader.addr + offset), PDE_BASE(pde)))) {
                RET_MSG(NULL, "Unable to read PTE.");
            }

            if(!pte.is_present) {
                pte.is_user = 1;
                pte.is_read_write = IS_FLAG_SET(sheader.flags, SHF_WRITE);
                pte.is_present = 1;

                if(sheader.type == SHT_PROGBITS) {
                    pte.base = (pbase_t)ADDR_TO_PBASE((uint32_t)img + sheader.offset + offset);
                } else if(sheader.type == SHT_NOBITS) {
                    page = alloc_phys_frame();

                    if(page == INVALID_PADDR) {
                        RET_MSG(NULL, "load_elf_exe(): No more physical pages are available.");
                    }

                    if(clear_phys_frames(page, 1) != 1) {
                        kprintf("Unable to clear page for SHT_NOBITS section");
                    }

                    pte.base = (pbase_t)ADDR_TO_PBASE(page);
                    pte.was_allocated = 1;
                } else
                    continue;

                if(IS_ERROR(write_pte(PTE_INDEX(sheader.addr + offset), pte, PDE_BASE(pde)))) {
                    RET_MSG(NULL, "load_elf_exe(): Unable to write PDE");
                }
            } else if(sheader.type == SHT_NOBITS) {
                memset((void*)(sheader.addr + offset), 0, PAGE_SIZE - (offset % PAGE_SIZE));
            }
        }
    }

    return thread;
}

struct InitStackArgs {
    uint32_t return_address;
    multiboot_info_t* multiboot_info;
    uint64_t first_free_page;
    size_t stack_size;
    char fxsave_state[512];
    unsigned char code[4];
};

/**
 Bootstraps the initial server and passes necessary boot data to it.
 */
int bootstrap_init_server(multiboot_info_t* info)
{
    addr_t init_server_stack = (addr_t)INIT_SERVER_STACK_TOP;
    paddr_t init_server_pdir = INVALID_PADDR;
    elf_header_t elf_header;
    size_t init_stack_size = 0;

    kprintfln("Bootstrapping initial server...");

    if(peek(init_server_img, &elf_header, sizeof elf_header) != E_OK) {
        goto failed_bootstrap;
    }

    if(!is_valid_elf_exe(&elf_header)) {
        kprintfln("Invalid ELF exe");
        goto failed_bootstrap;
    }

    if((init_server_pdir = alloc_phys_frame()) == INVALID_PADDR) {
        kprintfln("Unable to create page directory for initial server.");
        goto failed_bootstrap;
    }

    if(clear_phys_frames(init_server_pdir, 1) != 1) {
        kprintfln("Unable to clear init server page directory.");
        goto failed_bootstrap;
    }

    pde_t pde;
    pte_t pte;

    memset(&pde, 0, sizeof(pde_t));
    memset(&pte, 0, sizeof(pte_t));

    addr_t stack_ptab = alloc_phys_frame();

    if(stack_ptab == INVALID_PADDR) {
        kprintfln("Unable to initialize stack page table");
        goto failed_bootstrap;
    } else {
        if(clear_phys_frames(stack_ptab, 1) != 1) {
            goto failed_bootstrap;
        }
    }

    pde.base = (pbase_t)PADDR_TO_PBASE(stack_ptab);
    pde.is_read_write = 1;
    pde.is_user = 1;
    pde.is_present = 1;
    pde.was_allocated = 1;

    if(IS_ERROR(
        write_pde(PDE_INDEX(init_server_stack - PAGE_TABLE_SIZE), pde, init_server_pdir))) {
        goto failed_bootstrap;
    }

    for(size_t p = 0; p < (512 * 1024) / PAGE_SIZE; p++) {
        addr_t stack_page = alloc_phys_frame();

        if(stack_page == INVALID_PADDR) {
            kprintfln("Unable to initialize stack page.");
            goto failed_bootstrap;
        }

        pte.base = (pbase_t)PADDR_TO_PBASE(stack_page);
        pte.is_read_write = 1;
        pte.is_user = 1;
        pte.is_present = 1;
        pte.was_allocated = 1;

        if(IS_ERROR(
            write_pte(PTE_INDEX(init_server_stack - (p + 1) * PAGE_SIZE), pte, stack_ptab))) {
            if(p == 0) {
                kprintfln("Unable to write page map entries for init server stack.");
                goto failed_bootstrap;
            } else
                break;
        }

        init_stack_size += PAGE_SIZE;
    }

    // Let the initial server have access to conventional memory (first 640 KiB)

    if(IS_ERROR(read_pde(&pde, 0, init_server_pdir))) {
        goto failed_bootstrap;
    }

    addr_t ptab;

    if(!pde.is_present) {
        addr_t new_ptab = alloc_phys_frame();

        if(new_ptab == INVALID_PADDR)
            goto failed_bootstrap;

        if(clear_phys_frames(new_ptab, 1) != 1) {
            goto failed_bootstrap;
        }

        pde.value = 0;
        pde.is_present = 1;
        pde.is_read_write = 1;
        pde.is_user = 1;
        pde.base = (pbase_t)PADDR_TO_PBASE(new_ptab);
        pde.was_allocated = 1;

        if(IS_ERROR(write_pde(0, pde, init_server_pdir))) {
            goto failed_bootstrap;
        }

        ptab = new_ptab;
    } else
        ptab = PDE_BASE(pde);

    for(size_t i = 0; i < PTE_INDEX(VGA_RAM); i++) {
        pte.value = 0;
        pte.is_present = 1;
        pte.is_read_write = 0;
        pte.is_user = 1;
        pte.base = i;

        if(IS_ERROR(write_pte(i, pte, ptab))) {
            goto failed_bootstrap;
        }
    }

    if((init_server_thread = load_elf_exe(
        init_server_img, init_server_pdir,
        (void*)(init_server_stack - sizeof(struct InitStackArgs))))
        == NULL) {
        kprintfln("Unable to load ELF executable.");
        goto failed_bootstrap;
    }

    kprintfln("Starting initial server... %#p", init_server_thread);

    if(IS_ERROR(thread_start(init_server_thread))) {
        goto release_init_thread;
    }

    struct InitStackArgs stack_data =
    {
      .return_address = init_server_stack
          - sizeof((struct InitStackArgs*)0)->code,
      .multiboot_info = (multiboot_info_t*)VIRT_TO_PHYS((addr_t)info),
      .first_free_page = first_free_page,
      .stack_size = init_stack_size,
      .fxsave_state = { 0 },
      .code = {
        0xF4,   // hlt
        0x0F,
        0x90,   // nop
        0x0B    // ud2
      }
    };

    memset(stack_data.fxsave_state, 0, sizeof stack_data.fxsave_state);
    init_server_thread->fxsave_state = (fxsave_state_t *)((size_t)init_server_stack - sizeof(stack_data) + ((uintptr_t)&stack_data.fxsave_state - (uintptr_t)&stack_data));

    can_alloc_frames = false;

    if(IS_ERROR(poke_virt(init_server_stack - sizeof(stack_data), sizeof(stack_data), &stack_data, init_server_pdir))) {
        goto failed_bootstrap;
    }

    return E_OK;

release_init_thread:
    if(IS_ERROR(thread_release(init_server_thread))) {
        kprintfln("Unable to release thread.\n");
    }

failed_bootstrap:
    kprintfln("Unable to start initial server.");
    return E_FAIL;
}

