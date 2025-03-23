#include "init.h"
#include "memory.h"
#include <kernel/multiboot.h>
#include <kernel/memory.h>
#include <kernel/lock.h>

extern LOCKED_DECL(tcb_t *, tcb_table);

DISC_DATA paddr_t first_free_page;

DISC_CODE paddr_t alloc_phys_frame(void);

DISC_CODE bool is_reserved_page(paddr_t addr, multiboot_info_t* info,
    int is_page_sized);

DISC_CODE int init_memory(multiboot_info_t* info);
DISC_CODE static void setup_tss(unsigned int processor);
DISC_CODE void init_paging(void);

DISC_DATA ALIGN_AS(PAGE_SIZE) pmap_entry_t kpage_dir[PMAP_ENTRY_COUNT]; // The initial page directory used by the kernel on bootstrap

ALIGN_AS(PAGE_SIZE) pte_t kernel_page_table[PTE_ENTRY_COUNT];
ALIGN_AS(PAGE_SIZE) pte_t klower_mem_ptab[PTE_ENTRY_COUNT];

extern gdt_entry_t kernel_gdt[8];
extern multiboot_info_t* multiboot_info;

extern uint32_t lapic_ptr;
extern uint32_t ioapic_ptr;

paddr_t alloc_phys_frame(void)
{
    paddr_t frame = first_free_page;

    while(frame >= EXTENDED_MEMORY && is_reserved_page(frame, multiboot_info, 0)) {
        frame += PAGE_SIZE;
    }

    if(frame < EXTENDED_MEMORY)
        return INVALID_PADDR;

    first_free_page = frame + PAGE_SIZE;

    return frame;
}

#define OVERLAPS(start, end, region_start, region_len) ((start) >= (region_start) && (addr) < (region_start) + (region_len)) || ((end) >= (region_start) && (end) < (region_start) + (region_len))

bool is_reserved_page(paddr_t addr, multiboot_info_t* info, int is_large_page)
{
    paddr_t kernel_start = (paddr_t)&kphys_start;
    unsigned int kernel_length = (unsigned int)ksize;
    paddr_t addr_end;

    if(is_large_page) {
        addr = ALIGN_DOWN(addr, PAGE_TABLE_SIZE);
        addr_end = addr + PAGE_TABLE_SIZE;
    } else {
        addr = ALIGN_DOWN(addr, PAGE_SIZE);
        addr_end = addr + PAGE_SIZE;
    }

    // Physical Memory within the first 1 MiB and memory within the kernel are reserved.

    if(addr < (paddr_t)EXTENDED_MEMORY || OVERLAPS(addr, addr_end, kernel_start, kernel_length)) {
        return true;
    } else {
        /* Memory that's not specifically marked as 'available' in the
           multiboot memory map is also reserved. */

        bool in_memory_map = false;
        const memory_map_t* mmap;

        for(unsigned int offset = 0; offset < info->mmap_length; offset += mmap->size + sizeof(mmap->size)) {
            mmap = (const memory_map_t*)PHYS_TO_VIRT(info->mmap_addr + offset);

            uint64_t mmap_len;
            uint64_t mmap_base;

            mmap_len = (((uint64_t)mmap->length_high << 32) | (uint64_t)mmap->length_low);
            mmap_base = (((uint64_t)mmap->base_addr_high << 32) | (uint64_t)mmap->base_addr_high);
            
            if(OVERLAPS((uint64_t)addr, (uint64_t)addr_end, (uint64_t)mmap_base, (uint64_t)mmap_len)) {
                in_memory_map = true;

                if(mmap->type != MBI_TYPE_AVAIL) {
                    return true;
                }

                break;
            }
        }

        // Regions of non-existent memory are considered to be reserved.

        if(!in_memory_map)
            return true;

        // Check the modules

        module_t* module = (module_t*)PHYS_TO_VIRT(info->mods_addr);

        for(unsigned int i = 0; i < info->mods_count; i++, module++) {
            if(OVERLAPS(addr, addr_end, module->mod_start, module->mod_end - module->mod_start)) {
                return true;
            }
        }

        return false;
    }
}

void setup_tss(unsigned int processor)
{
    gdt_entry_t* tss_desc = &kernel_gdt[TSS_SEL / sizeof(gdt_entry_t)];
    size_t tss_limit = sizeof tss;

    tss_desc->base1 = (uint32_t)&tss & 0xFFFFu;
    tss_desc->base2 = (uint8_t)(((uint32_t)&tss >> 16) & 0xFFu);
    tss_desc->base3 = (uint8_t)(((uint32_t)&tss >> 24) & 0xFFu);
    tss_desc->limit1 = (uint16_t)(tss_limit & 0xFFFF); // Size of TSS structure and IO Bitmap (in pages)
    tss_desc->limit2 = (uint8_t)(tss_limit >> 16);

    // TODO: Set the kernel stack that will be shared between threads (one per processor)

    tss.ss0 = KDATA_SEL;
    tss.esp0 = (uint32_t)kernel_stack_top;

    __asm__ __volatile__("ltr %%ax" :: "a"(TSS_SEL));
}

int init_memory(multiboot_info_t* info)
{
    first_free_page = (paddr_t)((uintptr_t)&kphys_start + (size_t)&ksize);

    unsigned int total_phys_mem = (info->mem_upper + 1024) * 1024;

    kprintfln("Total Memory: %#x. %d pages.", total_phys_mem,
        total_phys_mem / PAGE_SIZE);

    if(total_phys_mem < (64 << 20)) {
        kprintfln("Not enough memory. System must have at least 64 MiB of memory.");
        return E_FAIL;
    }

    kprintfln("Kernel page directory: %#p", kpage_dir);

#ifdef DEBUG
    kprintfln("TCB Table: %#p size: %d bytes", LOCK_VAL(tcb_table), MAX_THREADS * sizeof(tcb_t));
    kprintfln("First free page: %#lx", (addr_t)first_free_page);
#endif /* DEBUG */

    return E_OK;
}

// Only code/data in the .boot segment can be accesesed without faulting until paging is
// enabled.

void init_paging(void)
{
    uint32_t old_cr0;

    // These are supposed to be cleared, but just in case...

    for(size_t i = 0; i < 1024; i++) {
        kpage_dir[i].value = 0;
        kernel_page_table[i].value = 0;
        klower_mem_ptab[i].value = 0;
    }

    /* Map the first MiB of physical memory. */
    for(size_t i = 0; i < EXTENDED_MEMORY / PAGE_SIZE; i++) {
        bool is_mmio = i >= VGA_RAM / PAGE_SIZE;
        klower_mem_ptab[i].value = 0;
        klower_mem_ptab[i].is_present = 1;
        klower_mem_ptab[i].is_read_write = 1;
        klower_mem_ptab[i].pat = is_mmio ? 1 : 0;
        klower_mem_ptab[i].pcd = is_mmio ? 1 : 0;
        klower_mem_ptab[i].pwt = is_mmio ? 1 : 0;
        klower_mem_ptab[i].base = i;
    }

    // Map to KERNEL_LOWER_MEM vaddr

    pde_t* pde = &kpage_dir[PDE_INDEX(KERNEL_LOWER_MEM)].pde;

    pde->base = PADDR_TO_PBASE((paddr_t)((uintptr_t)klower_mem_ptab + (uintptr_t)KERNEL_START - (uintptr_t)KERNEL_VSTART));
    pde->is_present = 1;
    pde->is_read_write = 1;

    // Identity map the first 4 MiB

    pde = &kpage_dir[0].pde;

    pde->base = 0;
    pde->is_page_sized = 1;
    pde->is_read_write = 1;
    pde->is_present = 1;

    // Map the kernel memory

    pde = &kpage_dir[PDE_INDEX(KERNEL_VSTART)].pde;
    pde->base = PADDR_TO_PBASE((paddr_t)((uintptr_t)kernel_page_table + (uintptr_t)KERNEL_START - (uintptr_t)KERNEL_VSTART));
    pde->is_present = 1;
    pde->is_read_write = 1;
    
    paddr_t phys = (paddr_t)KERNEL_START;
    addr_t virt = (addr_t)KERNEL_VSTART;

    for(; phys < KERNEL_START + ksize; phys += PAGE_SIZE, virt += PAGE_SIZE) {
        pte_t *pte = &kernel_page_table[PTE_INDEX(virt)];

        pte->global = 1;
        pte->base = PADDR_TO_PTE_BASE(phys);

        pte->is_read_write = ((virt < (addr_t)kcode) || (virt >= (addr_t)kdata)) ? 1 : 0;
        pte->is_present = 1;
    }

    // Recursively map the page directory into itself.

    pde_t* kmap_pde = &kpage_dir[KERNEL_RECURSIVE_PDE].pde;

    kmap_pde->base = PADDR_TO_PBASE((addr_t)((uintptr_t)kpage_dir + (uintptr_t)KERNEL_START - (uintptr_t)KERNEL_VSTART));

    kmap_pde->is_read_write = 1;
    kmap_pde->is_present = 1;

    // Set the page directory

    __asm__("mov %0, %%cr3" :: "r"((paddr_t)(addr_t)((uintptr_t)kpage_dir + (uintptr_t)KERNEL_START - (uintptr_t)KERNEL_VSTART)));

    // Enable debugging extensions, large pages, global pages,
    // FXSAVE/FXRSTOR, and SIMD exceptions

    __asm__("mov %0, %%cr4" :: "r"(CR4_DE | CR4_PSE | CR4_PGE
        | CR4_OSFXSR | CR4_OSXMMEXCPT));

    // Enable paging

    __asm__("mov %%cr0, %0" : "=r"(old_cr0));
    __asm__("mov %0, %%cr0" :: "r"(old_cr0 | CR0_PG | CR0_MP) : "memory");

    // Reload segment registers

    __asm__("mov %0, %%ds\n"
        "mov %0, %%es\n"
        "mov %1, %%fs\n"
        "mov %1, %%gs\n" :: "r"(KDATA_SEL), "r"(0) : "memory");

    __asm__("push %%eax\n"
        "pushl %0\n"
        "mov $1f, %%eax\n"
        "push %%eax\n"
        "retf\n"
        "1:\n"
        "pop %%eax\n" :: "i"(KCODE_SEL) : "memory", "cc");

    setup_tss(0);
}
