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
DISC_CODE static void bootstrap_init_server(multiboot_info_t* info);
DISC_DATA static addr_t init_server_img;

extern tcb_t* init_server_thread;
extern addr_t first_free_page;

int load_servers(multiboot_info_t* info)
{
    module_t* module;
    bool init_server_found = false;

    kprintf("Boot info struct: %#p\nModules located at %#p. %d modules found.\n",
        (void*)KVIRT_TO_PHYS((addr_t)info), info->mods_addr, info->mods_count);

    module = (module_t*)KPHYS_TO_VIRT(info->mods_addr);

    /* Locate the initial server module. */

    for(size_t i = info->mods_count; i; i--, module++) {
        if(i == info->mods_count)
            init_server_img = (addr_t)module->mod_start;

        if(module->string) {
            const char* str_ptr = (const char*)KPHYS_TO_VIRT(module->string);

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
            kprintf(
                "Initial server was not specified with --init. Using first module.\n");
        else {
            kprintf("Can't find initial server.");
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

    peek(img, &image, sizeof image);
    pte.is_present = 0;

    if(!is_valid_elf_exe(&image)) {
        kprintf("Not a valid ELF executable.\n");
        return NULL;
    }

    thread = create_thread((void*)image.entry, addr_space, user_stack);

    if(thread == NULL) {
        kprintf("loadElfExe(): Couldn't create thread.\n");
        return NULL;
    }

    /* Create the page table before mapping memory */

    for(i = 0; i < image.shnum; i++) {
        peek((img + image.shoff + i * image.shentsize), &sheader, sizeof sheader);

        if(!(sheader.flags & SHF_ALLOC))
            continue;

        for(offset = 0; offset < sheader.size; offset += PAGE_SIZE) {
            pde = read_pde(PDE_INDEX(sheader.addr + offset), addr_space);

            if(!pde.is_present) {
                page = alloc_page_frame();

                if(page == INVALID_PFRAME)
                    RET_MSG(NULL, "load_elf_exe(): Unable to allocate PDE.");

                clear_phys_page(page);

                pde.base = (uint32_t)ADDR_TO_PFRAME(page);
                pde.is_read_write = 1;
                pde.is_user = 1;
                pde.is_present = 1;

                if(IS_ERROR(
                    write_pde(PDE_INDEX(sheader.addr + offset), pde, addr_space)))
                    RET_MSG(NULL, "loadElfExe(): Unable to write PDE.");
            } else if(pde.is_large_page)
                RET_MSG(
                    NULL,
                    "loadElfExe(): Memory region has already been mapped to a large page.");

            pte = read_pte(PTE_INDEX(sheader.addr + offset), PDE_BASE(pde));

            if(!pte.is_present) {
                pte.is_user = 1;
                pte.is_read_write = IS_FLAG_SET(sheader.flags, SHF_WRITE);
                pte.is_present = 1;

                if(sheader.type == SHT_PROGBITS)
                    pte.base = (uint32_t)ADDR_TO_PFRAME(
                        (uint32_t)img + sheader.offset + offset);
                else if(sheader.type == SHT_NOBITS) {
                    page = alloc_page_frame();

                    if(page == INVALID_PFRAME)
                        RET_MSG(NULL,
                            "loadElfExe(): No more physical pages are available.");

                    clear_phys_page(page);
                    pte.base = (uint32_t)ADDR_TO_PFRAME(page);
                } else
                    continue;

                if(IS_ERROR(
                    write_pte(PTE_INDEX(sheader.addr + offset), pte, PDE_BASE(pde))))
                    RET_MSG(NULL, "loadElfExe(): Unable to write PDE");
            } else if(sheader.type == SHT_NOBITS)
                memset((void*)(sheader.addr + offset), 0,
                    PAGE_SIZE - (offset % PAGE_SIZE));
        }
    }

    return thread;
}

struct InitStackArgs {
    uint32_t return_address;
    multiboot_info_t* multiboot_info;
    addr_t first_free_page;
    size_t stack_size;
    unsigned char code[4];
};

/**
 Bootstraps the initial server and passes necessary boot data to it.
 */

void bootstrap_init_server(multiboot_info_t* info)
{
    addr_t init_server_stack = (addr_t)INIT_SERVER_STACK_TOP;
    addr_t init_server_pdir = INVALID_PFRAME;
    elf_header_t elf_header;

    /* code:

     xor    eax, eax
     xor    ebx, ebx
     inc    ebx
     int    0xFF     # sys_exit(1)
     nop
     nop
     nop
     ud2             # Trigger Invalid Opcode Exception: #UD
     */

    struct InitStackArgs stack_data =
    {
      .return_address = init_server_stack
          - sizeof((struct InitStackArgs*)0)->code,
      .multiboot_info = (multiboot_info_t*)KVIRT_TO_PHYS((addr_t)info),
      .first_free_page = first_free_page,
      .stack_size = 0,
      .code = {
        0x90,
        0x90,
        0x0F,
        0x0B
      }
    };

    kprintf("Bootstrapping initial server...\n");

    peek(init_server_img, &elf_header, sizeof elf_header);

    if(!is_valid_elf_exe(&elf_header)) {
        kprintf("Invalid ELF exe\n");
        goto failed_bootstrap;
    }

    if((init_server_pdir = alloc_page_frame()) == INVALID_PFRAME) {
        kprintf("Unable to create page directory for initial server.\n");
        goto failed_bootstrap;
    }

    if(clear_phys_page(init_server_pdir) != E_OK) {
        kprintf("Unable to clear init server page directory.\n");
        goto failed_bootstrap;
    }

    pde_t pde;
    pte_t pte;

    memset(&pde, 0, sizeof(pde_t));
    memset(&pte, 0, sizeof(pte_t));

    addr_t stack_ptab = alloc_page_frame();

    if(stack_ptab == INVALID_PFRAME) {
        kprintf("Unable to initialize stack page table\n");
        goto failed_bootstrap;
    } else
        clear_phys_page(stack_ptab);

    pde.base = (uint32_t)ADDR_TO_PFRAME(stack_ptab);
    pde.is_read_write = 1;
    pde.is_user = 1;
    pde.is_present = 1;

    if(IS_ERROR(
        write_pde(PDE_INDEX(init_server_stack - PAGE_TABLE_SIZE), pde, init_server_pdir))) {
        goto failed_bootstrap;
    }

    for(size_t p = 0; p < (512 * 1024) / PAGE_SIZE; p++) {
        addr_t stack_page = alloc_page_frame();

        if(stack_page == INVALID_PFRAME) {
            kprintf("Unable to initialize stack page.\n");
            goto failed_bootstrap;
        }

        pte.base = (uint32_t)ADDR_TO_PFRAME(stack_page);
        pte.is_read_write = 1;
        pte.is_user = 1;
        pte.is_present = 1;

        if(IS_ERROR(
            write_pte(PTE_INDEX(init_server_stack - (p + 1) * PAGE_SIZE), pte, stack_ptab))) {
            if(p == 0) {
                kprintf("Unable to write page map entries for init server stack.\n");
                goto failed_bootstrap;
            } else
                break;
        }

        if(p == 0) {
            poke(stack_page + PAGE_SIZE - sizeof(stack_data), &stack_data,
                sizeof(stack_data));
        }

        stack_data.stack_size += PAGE_SIZE;
    }

    // Let the initial server have access to conventional memory (first 640 KiB)

    pde = read_pde(0, init_server_pdir);
    addr_t ptab;

    if(!pde.is_present) {
        addr_t new_ptab = alloc_page_frame();

        if(new_ptab == INVALID_PFRAME)
            goto failed_bootstrap;

        clear_phys_page(new_ptab);

        pde.value = 0;
        pde.is_present = 1;
        pde.is_read_write = 1;
        pde.is_user = 1;
        pde.base = ADDR_TO_PFRAME(new_ptab);

        if(IS_ERROR(write_pde(0, pde, init_server_pdir)))
            goto failed_bootstrap;

        ptab = new_ptab;
    } else
        ptab = PDE_BASE(pde);

    for(size_t i = 0; i < PTE_INDEX(VGA_RAM); i++) {
        pte.value = 0;
        pte.is_present = 1;
        pte.is_read_write = 0;
        pte.is_user = 1;
        pte.base = i;

        if(IS_ERROR(write_pte(i, pte, ptab)))
            goto failed_bootstrap;
    }

    if((init_server_thread = load_elf_exe(
        init_server_img, init_server_pdir,
        (void*)(init_server_stack - sizeof(stack_data))))
        == NULL) {
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

