.code32
.include "asm/kernel.h"

.data

EXPORT kernelIDT
        .rept KERNEL_IDT_LEN
          .byte 0
        .endr
EXPORT kernelGDT
	.quad	0			// Null entry
	.quad	0x00CF98000000FFFF	// Kernel Code
	.quad	0x00CF92000000FFFF	// Kernel Data
	.quad	0x00CFF8000000FFFF	// User Code
	.quad	0x00CFF2000000FFFF	// User Data
	.quad	0x0000890000000000	// TSS (base & limit must be set by kernel)
	.quad      0x00CF98000000FFFF | (((KERNEL_PHYS_START-KERNEL_VIRT_START) & 0xFFFFFF) << 16) | (((KERNEL_PHYS_START-KERNEL_VIRT_START) & 0xFF000000) << 32)	// Bootstrap Code
	.quad      0x00CF92000000FFFF | (((KERNEL_PHYS_START-KERNEL_VIRT_START) & 0xFFFFFF) << 16) | (((KERNEL_PHYS_START-KERNEL_VIRT_START) & 0xFF000000) << 32)	// Bootstrap Data

EXPORT kernelTSS
        .int 0
EXPORT tssEsp0
	.int 0
	.int KERNEL_DATA_SEL
	.rept 22
	  .int 0
        .endr
        .word 0, KERNEL_TSS_LEN + TSS_IO_PERM_BMP_LEN

.section .pages, "aw", @nobits
.align 4096

.ifdef ASM_USCORE
.global _ioPermBitmap
.lcomm _ioPermBitmap, TSS_IO_PERM_BMP_LEN
.else
.global ioPermBitmap
.lcomm ioPermBitmap, TSS_IO_PERM_BMP_LEN
.endif
