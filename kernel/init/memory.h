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
#include <kernel/multiboot2.h>

extern addr_t alloc_page_frame(void);

extern bool is_reserved_page(uint64_t addr, const struct multiboot_info_header *header);

extern int init_memory(const struct multiboot_info_header *header);

#endif /* KERNEL_INIT_MEMORY_H */
