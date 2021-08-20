.ifndef KERNEL_H
.set KERNEL_H, 1

.include "asm/asm.h"

.equ KERNEL_DPL, 0x00
.equ USER_DPL, 0x03

.equ KERNEL_TCB_START, 0xC0000000
.equ KERNEL_VIRT_START, 0xC0800000
.equ KERNEL_PHYS_START, 0x00100000
.equ KERNEL_CODE_SEL, (0x08 | KERNEL_DPL)
.equ KERNEL_DATA_SEL, (0x10 | KERNEL_DPL)
.equ USER_CODE_SEL, (0x18 | USER_DPL)
.equ USER_DATA_SEL, (0x20 | USER_DPL)
.equ KERNEL_TSS_SEL, (0x28 | KERNEL_DPL)
.equ BOOT_CODE_SEL, (0x30 | KERNEL_DPL)
.equ BOOT_DATA_SEL, (0x38 | KERNEL_DPL)
.equ KERNEL_RESD_TABLES,	4	// for page tables/directory, temporary page tables 1 & 2, and physical memory mapping
.equ PAGE_SIZE,		4096

.equ KERNEL_IDT_LEN,		0x800
.equ KERNEL_GDT_LEN,		0x40
.equ KERNEL_TSS_LEN,		0x68
.equ TSS_IO_PERM_BMP_LEN,	2*4096

.data

IMPORT kCode
IMPORT kData
IMPORT kBss
IMPORT kEnd
IMPORT kdCode
IMPORT kdData
IMPORT kPhysStart
IMPORT kVirtStart
IMPORT kPhysData
IMPORT kVirtData
IMPORT kPhysBss
IMPORT kVirtBss
IMPORT kSize
IMPORT kPhysToVirt
IMPORT kVirtToPhys
IMPORT VPhysMemStart

/*
.equ kBootStackTop,  0x9F000
.equ kPageDir,       0x90000
.equ kLowPageTab,    0x91000
.equ kPageTab,       0x92000
.equ k1to1PageTab,   0x93000
*/

.endif
