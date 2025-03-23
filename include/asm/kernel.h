.ifndef KERNEL_H
.set KERNEL_H, 1

#include <asm/asm.h>

#define KERNEL_DPL 0x00
#define USER_DPL 0x03

#define KERNEL_VIRT_START 0xF8100000
#define KERNEL_PHYS_START 0x100000
#define KERNEL_CODE_SEL (0x08 | KERNEL_DPL)
#define KERNEL_DATA_SEL (0x10 | KERNEL_DPL)
#define USER_CODE_SEL (0x18 | USER_DPL)
#define USER_DATA_SEL (0x20 | USER_DPL)
#define KERNEL_TSS_SEL (0x28 | KERNEL_DPL)

#define BOOT_CODE_SEL (0x30 | KERNEL_DPL)
#define BOOT_DATA_SEL (0x38 | KERNEL_DPL)

#define KERNEL_RESD_TABLES	4	// for page tables/directory, temporary page tables 1 & 2, and physical memory mapping
#define PAGE_SIZE		4096

#define KERNEL_IDT_LEN		0x800
#define KERNEL_GDT_LEN		0x40
#define KERNEL_TSS_LEN		0x68
#define TSS_IO_PERM_BMP_LEN	2*4096

.endif
