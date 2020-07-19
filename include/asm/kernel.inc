%ifndef KERNEL_INC
%define KERNEL_INC

%include "asm/asm.inc"

%define KERNEL_VIRT_START       0x80000000
%define KERNEL_PHYS_START       0x00100000
%define KERNEL_CODE_SEL		0x08
%define KERNEL_DATA_SEL		0x10
%define USER_CODE_SEL		(0x18 | 0x03)
%define USER_DATA_SEL		(0x20 | 0x03)
%define KERNEL_TSS_SEL		0x28
%define BOOT_CODE_SEL		0x30
%define BOOT_DATA_SEL		0x38
%define KERNEL_RESD_TABLES	4	; for page tables/directory, temporary page tables 1 & 2, and physical memory mapping
%define PAGE_SIZE		4096

%define KERNEL_IDT_LEN		0x800
%define KERNEL_GDT_LEN		0x40
%define KERNEL_VARS_LEN		4096
%define KERNEL_STACK_LEN	4096
%define KERNEL_STACK		4096
%define IDLE_STACK_LEN		4096
%define IRQ_STACK_LEN     8192
%define KERNEL_BOOT_STACK_LEN	4096
%define KERNEL_TSS_LEN		0x68
%define TSS_IO_PERM_BMP_LEN	2*4096

[section .data]
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

;kCodeSel  equ 0x08
;kDataSel  equ 0x10
;uCodeSel  equ 0x1B
;uDataSel  equ 0x23
;tssSel    equ 0x28
;bootCodeSel equ 0x30
;bootDataSel equ 0x38
;kResdTables equ 4

kBootStackTop equ 0xA0000
kPageDir      equ 0x1000
idleStackTop  equ 0x1000
kLowPageTab   equ 0x2000
kPageTab      equ 0x3000
k1to1PageTab  equ 0x4000

;_kernelIDTLen equ 0x800
;_kernelGDTLen equ 0x40
;_kernelTSSLen equ 0x068
;_kernelBootStackLen equ 1024
;_idleStackLen equ 4096
;_kernelStackLen equ 4096
;_kernelVarsLen equ 4096
%endif
