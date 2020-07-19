[BITS 32]
%include "asm/kernel.inc"

[section .data]

EXPORT kernelIDT
	times KERNEL_IDT_LEN db 0
EXPORT kernelGDT
	dq	0			; Null entry
	dq	0x00CF98000000FFFF	; Kernel Code
	dq	0x00CF92000000FFFF	; Kernel Data
	dq	0x00CFF8000000FFFF	; User Code
	dq	0x00CFF2000000FFFF	; User Data
	dq	0x0000890000000000	; TSS (base & limit must be set by kernel)
	dq      0x00CF98000000FFFF | (((KERNEL_PHYS_START-KERNEL_VIRT_START) & 0xFFFFFF) << 16) | (((KERNEL_PHYS_START-KERNEL_VIRT_START) & 0xFF000000) << 32)	; Bootstrap Code
	dq      0x00CF92000000FFFF | (((KERNEL_PHYS_START-KERNEL_VIRT_START) & 0xFFFFFF) << 16) | (((KERNEL_PHYS_START-KERNEL_VIRT_START) & 0xFF000000) << 32)	; Bootstrap Data

EXPORT kernelTSS
        dd 0
EXPORT tssEsp0
	dd 0
	dd KERNEL_DATA_SEL
	times 22 dd 0
        dw 0, KERNEL_TSS_LEN + TSS_IO_PERM_BMP_LEN

[section .pages nobits alloc noexec write align=4096]
EXPORT ioPermBitmap
	resb TSS_IO_PERM_BMP_LEN
EXPORT idleStack
	resb IDLE_STACK_LEN
EXPORT kernelStack
	resb KERNEL_STACK_LEN
;EXPORT initKrnlPDir
;	resb PAGE_SIZE
;EXPORT kernelVars
;	resb KERNEL_VARS_LEN
;EXPORT lowMemPTab
;	resb PAGE_SIZE
;EXPORT kernelPTab
;	resb PAGE_SIZE

[section .dpages progbits alloc noexec write align=4096]
EXPORT lowMemPTab
	times PAGE_SIZE		db 0

[section .data]
EXPORT initKrnlPDir
  dd (ioPermBitmap + ~KERNEL_VIRT_START + 1) + KERNEL_PHYS_START
EXPORT kernelVars
  dd (ioPermBitmap + ~KERNEL_VIRT_START + 1) + KERNEL_PHYS_START + PAGE_SIZE
