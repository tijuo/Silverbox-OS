#ifndef KERNEL_INIT_MEMORY_H
#define KERNEL_INIT_MEMORY_H

#define EBDA                0x80000u
#define VGA_RAM             0xA0000u
#define VGA_COLOR_TEXT      0xB8000u
#define ISA_EXT_ROM         0xC0000u
#define BIOS_EXT_ROM        0xE0000u
#define BIOS_ROM            0xF0000u
#define EXTENDED_MEMORY     0x100000u

#include <kernel/mm.h>

extern addr_t alloc_page_frame(void);

extern bool is_reserved_page(uint64_t addr, multiboot_info_t* info,
    int is_large_page);

extern int init_memory(multiboot_info_t* info);

#endif /* KERNEL_INIT_MEMORY_H */
