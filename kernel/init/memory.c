#include "init.h"
#include "memory.h"
#include <kernel/multiboot.h>
#include <kernel/memory.h>

DISC_DATA addr_t first_free_page;

DISC_CODE addr_t alloc_page_frame(void);

DISC_CODE bool is_reserved_page(uint64_t addr, multiboot_info_t* info,
    int is_large_page);

DISC_CODE int init_memory(multiboot_info_t* info);
DISC_CODE static void setup_gdt(void);
DISC_CODE void init_paging(void);

DISC_DATA ALIGNED(PAGE_SIZE) pmap_entry_t kpage_dir[PMAP_ENTRY_COUNT]; // The initial page directory used by the kernel on bootstrap

extern pte_t kmap_area_ptab[PTE_ENTRY_COUNT];
extern gdt_entry_t kernel_gdt[8];
extern multiboot_info_t* multiboot_info;

addr_t alloc_page_frame(void)
{
    addr_t frame = first_free_page;

    while(frame >= EXTENDED_MEMORY && is_reserved_page(frame, multiboot_info, 0)) {
        frame += PAGE_SIZE;
    }

    if(frame < EXTENDED_MEMORY)
        return INVALID_PFRAME;

    first_free_page = frame + PAGE_SIZE;

    return frame;
}

bool is_reserved_page(uint64_t addr, multiboot_info_t* info, int is_large_page)
{
    unsigned int kernel_start = (unsigned int)&kphys_start;
    unsigned int kernel_length = (unsigned int)ksize;
    uint64_t addr_end;

    if(is_large_page) {
        addr = addr & ~(PAGE_TABLE_SIZE - 1);
        addr_end = addr + PAGE_TABLE_SIZE;
    } else {
        addr = addr & ~(PAGE_SIZE - 1);
        addr_end = addr + PAGE_SIZE;
    }

    if(addr < (uint64_t)EXTENDED_MEMORY)
        return true;
    else if((addr >= kernel_start && addr < kernel_start + kernel_length)
        || (addr_end >= kernel_start && addr_end < kernel_start + kernel_length))
        return true;
    else if(addr >= (addr_t)EXT_PTR(ktcb_start)
        && addr < (addr_t)EXT_PTR(ktcb_end))
        return true;
    else {
        int in_some_region = 0;
        const memory_map_t* mmap;

        for(unsigned int offset = 0; offset < info->mmap_length;
            offset += mmap->size + sizeof(mmap->size)) {
            mmap = (const memory_map_t*)KPHYS_TO_VIRT(info->mmap_addr + offset);

            uint64_t mmap_len = mmap->length_high;
            mmap_len = mmap_len << 32;
            mmap_len |= mmap->length_low;

            uint64_t mmap_base = mmap->base_addr_high;
            mmap_base = mmap_base << 32;
            mmap_base |= mmap->base_addr_low;

            if(((uint64_t)addr >= mmap_base && (uint64_t)addr <= mmap_base + mmap_len) || ((uint64_t)addr_end
                >= mmap_base
                && (uint64_t)addr_end <= mmap_base
                + mmap_len)) {
                in_some_region = 1;

                if(mmap->type != MBI_TYPE_AVAIL)
                    return true;
                else
                    break;
            }
        }

        if(!in_some_region)
            return true;

        module_t* module = (module_t*)KPHYS_TO_VIRT(info->mods_addr);

        for(unsigned int i = 0; i < info->mods_count; i++, module++) {
            if(addr > module->mod_start && addr >= module->mod_end
                && addr_end > module->mod_start && addr_end > module->mod_end)
                continue;
            else if(addr < module->mod_start && addr < module->mod_end
                && addr_end <= module->mod_start && addr_end < module->mod_end)
                continue;
            else
                return true;
        }

        return false;
    }
}

void setup_gdt(void)
{
    gdt_entry_t* tss_desc = &kernel_gdt[TSS_SEL / sizeof(gdt_entry_t)];
    struct GdtPointer gdt_pointer = {
      .base = (uint32_t)kernel_gdt,
      .limit = 6 * sizeof(gdt_entry_t)
    };

    size_t tss_limit = sizeof tss;
    tss_desc->base1 = (uint32_t)&tss & 0xFFFFu;
    tss_desc->base2 = (uint8_t)(((uint32_t)&tss >> 16) & 0xFFu);
    tss_desc->base3 = (uint8_t)(((uint32_t)&tss >> 24) & 0xFFu);
    tss_desc->limit1 = (uint16_t)(tss_limit & 0xFFFF); // Size of TSS structure and IO Bitmap (in pages)
    tss_desc->limit2 = (uint8_t)(tss_limit >> 16);

    // Set the single kernel stack that will be shared between threads

    tss.ss0 = KDATA_SEL;
    tss.esp0 = (uint32_t)kernel_stack_top;

    __asm__ __volatile__("lgdt %0" :: "m"(gdt_pointer) : "memory");
    __asm__ __volatile__("ltr %%ax" :: "a"(TSS_SEL));
}

int init_memory(multiboot_info_t* info)
{
    first_free_page = KVIRT_TO_PHYS((addr_t)&ktcb_end);

    unsigned int total_phys_mem = (info->mem_upper + 1024) * 1024;

    if(IS_FLAG_SET(info->flags, MBI_FLAGS_MEM))
        kprintf("Lower Memory: %d bytes Upper Memory: %d bytes\n",
            info->mem_lower << 10, info->mem_upper << 10);
    kprintf("Total Memory: %#x. %d pages.\n", total_phys_mem,
        total_phys_mem / PAGE_SIZE);

    if(total_phys_mem < (64 << 20)) {
        kprintf("Not enough memory. System must have at least 64 MiB of memory.\n");
        return E_FAIL;
    }

    kprintf("Kernel AddrSpace: %#p\n", kpage_dir);

#ifdef DEBUG

    kprintf("TCB Table size: %d bytes\n", ktcb_table_size);
#endif /* DEBUG */

    return E_OK;
}

// Only code/data in the .boot segment can be accesesed without faulting until paging is
// enabled.

void init_paging(void)
{
    uint32_t old_cr0;
    pmap_entry_t* map_area_ptab = (pmap_entry_t*)KVIRT_TO_PHYS(
        (addr_t)kmap_area_ptab);

    // These are supposed to be cleared, but just in case...

    for(size_t i = 0; i < 1024; i++)
        kpage_dir[i].value = 0;

    for(size_t i = 0; i < 1024; i++)
        map_area_ptab[i].value = 0;

    large_pde_t* pde = &kpage_dir[0].large_pde;

    pde->base_lower = 0;
    pde->base_upper = 0;
    pde->is_large_page = 1;
    pde->is_present = 1;
    pde->is_read_write = 1;

    /* Map lower 120 MiB of physical memory to kernel space.
     * The kernel has to be careful not to write to read-only or non-existent areas.
     */

    for(addr_t addr = KERNEL_PHYS_START;
        addr < KMAP_AREA && addr >= KERNEL_PHYS_START; addr +=
        LARGE_PAGE_SIZE) {
        size_t pde_index = PDE_INDEX(addr);
        large_pde_t* large_pde = &kpage_dir[pde_index].large_pde;
        pframe_t pframe = ADDR_TO_PFRAME(KVIRT_TO_PHYS(addr));

        large_pde->global = 1;
        large_pde->base_lower = (uint32_t)((pframe >> 10) & 0x3FFu);
        large_pde->base_upper = (uint32_t)((pframe >> 20) & 0xFFu);

        large_pde->is_large_page = 1;
        large_pde->is_read_write = 1;
        large_pde->is_present = 1;
    }

    pde_t* kmap_pde = &kpage_dir[PDE_INDEX(KMAP_AREA2)].pde;

    kmap_pde->base = ADDR_TO_PFRAME(KVIRT_TO_PHYS((addr_t)kmap_area_ptab));

    kmap_pde->is_read_write = 1;
    kmap_pde->is_present = 1;

    // Set the page directory

    __asm__("mov %0, %%cr3" :: "r"((uint32_t)kpage_dir));

    // Enable debugging extensions, large pages, global pages,
    // FXSAVE/FXRSTOR, and SIMD exceptions

    __asm__("mov %0, %%cr4" :: "r"(CR4_DE | CR4_PSE | CR4_PGE
        | CR4_OSFXSR | CR4_OSXMMEXCPT));

    // Enable paging

    __asm__("mov %%cr0, %0" : "=r"(old_cr0));
    __asm__("mov %0, %%cr0" :: "r"(old_cr0 | CR0_PG | CR0_MP) : "memory");

    setup_gdt();

    // Reload segment registers

    __asm__("mov %0, %%ds\n"
        "mov %0, %%es\n"
        "mov %0, %%ss\n"
        "mov %1, %%fs\n"
        "mov %1, %%gs\n" :: "r"(KDATA_SEL), "r"(0) : "memory");

    __asm__("push %%eax\n"
        "pushl %0\n"
        "mov $1f, %%eax\n"
        "push %%eax\n"
        "retf\n"
        "1:\n"
        "pop %%eax\n" :: "i"(KCODE_SEL) : "memory", "cc");
}
