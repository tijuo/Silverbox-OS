#include <asm/asm.h>
#include <asm/grub.h>
#include <asm/kernel.h>

.code32

#define PG_PRESENT 1
#define PG_READ_WRITE 2
#define PG_PWT 8
#define PG_USER 4
#define PG_CD 16
#define PG_READ_ONLY 0
#define PG_SUPERVISOR 0
#define PG_GLOBAL 256

#define CR0_WP 0x10000
#define CR0_CD 0x40000000			// Global cache disable
#define CR0_NW 0x20000000			// Disable write-back/write-through (depends on processor)
#define CR0_PG 0x80000000

#define CR4_PGE 0x80
#define CR4_PSE 0x10

.section .header, "a", @progbits
.align 4

mboot:
        .int MULTIBOOT_HEADER_MAGIC
        .int MULTIBOOT_HEADER_FLAGS
        .int MULTIBOOT_CHECKSUM

        .int mboot
        .int kCode
        .int kBss
        .int kEnd
        .int start

	.int MULTIBOOT_EGA_TEXT
	.int MULTIBOOT_EGA_WIDTH
	.int MULTIBOOT_EGA_HEIGHT
	.int MULTIBOOT_EGA_DEPTH

// NOTE: Virtual addresses are used, but paging isn't enabled yet.
// GRUB loads the kernel to its physical address. Memory needs
// to be accessed *extra* carefully here until paging is enabled.

.section .dtext, "ax", @progbits
.align 16

EXPORT start
//  eax has the mulitboot magic value
//  ebx has multiboot data address

  lea	kBootStack, %esp
  add   $kVirtToPhys, %esp
  add   PAGE_SIZE, %esp
  mov	%esp, %ebp
  cld

// Save for main()

  push %eax
  push %ebx

  lea kernelGDT, %ecx
  add $kVirtToPhys, %ecx

// Load GDT

  push %ecx
  push $(7*8)
  lgdt 2(%esp)
  add  $8, %esp

 // Load valid linear addresses into the GDTR

  mov  $BOOT_DATA_SEL, %cx
  mov  %cx, %ds
  mov  %cx, %es
  mov  $KERNEL_DATA_SEL, %cx
  mov  %cx, %ss
  mov  %cx, %fs
  xor  %cx, %cx
  mov  %cx, %gs

  ljmp $BOOT_CODE_SEL, $.reloadSel

.reloadSel:

  cmp $MULTIBOOT_MAGIC, %eax
  jne .badMboot
  jmp .okMboot

.badMboot:
  jmp printMultibootErrMsg

.okMboot:
//  call initPaging2

/* Stack and base pointers point to 1:1 mapped addresses
   Convert them to their kernel memory equivalents since the
   1:1 mapped addresses will eventually be unmapped.
*/

  add $kPhysToVirt, %esp
  add $kPhysToVirt, %ebp

  call init
  jmp  idle

#define VIDEO_RAM_START 0xB8000
#define VIDEO_BIOS_START 0xC0000
/*
initPaging2:
  push %ebp
  mov %esp, %ebp

  mov %es, %ax
  push %eax
  mov %ds, %ax
  push %eax

  // Since we want to be writing to physical memory, use the
  // data selector that 1:1 maps linear memory to physical memory

  mov $KERNEL_DATA_SEL, %ax
  mov %ax, %es
  mov %ax, %ds

  // Clear the initial page directory, low memory page table, and kernel page table

  xor %eax, %eax
  mov $1024, %ecx
  lea kPageDir, %edi
  add $kVirtToPhys, %edi
  rep stosl

  mov $1024, %ecx
  lea kLowPageTab, %edi
  add $kVirtToPhys, %edi
  rep stosl

  mov $1024, %ecx
  lea kPageTab, %edi
  add $kVirtToPhys, %edi
  rep stosl

  mov $1024, %ecx
  lea k1to1PageTab, %edi
  add $kVirtToPhys, %edi
  rep stosl

  lea kLowPageTab, %ebx
  add $kVirtToPhys, %ebx
  add $4, %ebx
  mov $0x1000, %ecx		// Skip the first page (leave it as not present to catch NULL dereferences)

.mapLowMemLoop:
  cmp $VIDEO_RAM_START, %ecx
  jge  .mapVideoRam

  // Do not allow low memory to be overwritten

  mov %ecx, %eax
  or  $(PG_SUPERVISOR | PG_READ_ONLY | PG_PRESENT), %eax
  mov %eax, (%ebx)

  add $PAGE_SIZE, %ecx
  add $4, %ebx
  jmp .mapLowMemLoop

.mapVideoRam:
  mov $VIDEO_RAM_START, %ecx
  lea kLowPageTab, %ebx
  add $kVirtToPhys, %ebx
  mov $VIDEO_RAM_START, %edx
  shr $12, %edx
  and $0x3FF, %edx
  lea (%ebx,%edx,4), %ebx

.mapVideoRamLoop:
  cmp $VIDEO_BIOS_START, %ecx
  jge  .mapKernel

  mov %ecx, %eax
  or  $(PG_USER | PG_READ_WRITE | PG_CD | PG_PWT | PG_PRESENT), %eax
  mov %eax, (%ebx)

  add $PAGE_SIZE, %ecx
  add $4, %ebx
  jmp .mapVideoRamLoop

.mapKernel:
  lea kPageTab, %ebx
  add $kVirtToPhys, %ebx
  mov $kPhysStart, %esi
  mov $kVirtStart, %edi
  shr $12, %edi
  and $0x3FF, %edi
  lea (%ebx,%edi,4), %ebx
  xor %ecx, %ecx

.mapKernelLoop:
  cmp $kSize, %ecx
  jge  .mapPDEs

  mov %esi, %eax
  or  $(PG_SUPERVISOR | PG_READ_WRITE | PG_PRESENT), %eax
  mov %eax, (%ebx)

  add $PAGE_SIZE, %ecx
  add $PAGE_SIZE, %esi
  add $4, %ebx
  jmp .mapKernelLoop

.mapPDEs:
  lea kPageDir, %ebx
  add $kVirtToPhys, %ebx

  // Map page directory onto itself

  mov $0x3FF, %ecx
  lea (%ebx,%ecx,4), %edx
  lea kPageDir, %eax
  add $kVirtToPhys, %eax
  or  $(PG_SUPERVISOR | PG_READ_WRITE | PG_PRESENT), %eax
  mov %eax, (%edx)

  // Map first page table (1:1 mapping of low memory)

  xor %ecx, %ecx
  lea (%ebx,%ecx,4), %edx
  lea kLowPageTab, %eax
  add $kVirtToPhys, %eax
  or  $(PG_USER | PG_READ_WRITE | PG_PRESENT), %eax
  mov %eax, (%edx)

  // Map the page table for 1:1 kernel mapping (if necessary)

  mov  $kPhysStart, %ecx
  shr  $22, %ecx
  lea  (%ebx,%ecx,4), %edx
  push %ebx

  testl $PG_PRESENT, (%edx)
  jnz  .tablePresent
  xor  %edi, %edi
  lea  k1to1PageTab, %ebx
  add  $kVirtToPhys, %ebx
  jmp  .map1to1Kernel

.tablePresent:
  mov $1, %edi
  lea kLowPageTab, %ebx
  add $kVirtToPhys, %ebx

.map1to1Kernel:
  mov $kPhysStart, %ecx
  shr $12, %ecx
  and $0x3FF, %ecx
  lea (%ebx,%ecx,4), %ebx

  // If a page table was needed for 1:1 kernel mapping, then
  // map the kernel pages to the page table

  xor %ecx, %ecx
  mov $kPhysStart, %esi

.map1to1KernelLoop:
  cmp $kSize, %ecx
  jge  .restoreEBX

  mov %esi, %eax
  or  $(PG_SUPERVISOR | PG_READ_WRITE | PG_PRESENT), %eax
  mov %eax, (%ebx)

  add $PAGE_SIZE, %ecx
  add $PAGE_SIZE, %esi
  add $4, %ebx
  jmp .map1to1KernelLoop

.restoreEBX:
  pop %ebx
  test %edi, %edi
  jnz .setKPDE

.map1to1KernelPDE:
  lea  k1to1PageTab, %eax
  add $kVirtToPhys, %eax
  or   $(PG_SUPERVISOR | PG_READ_WRITE | PG_PRESENT), %eax
  mov  %eax, (%edx)

  // Map the kernel's page table

.setKPDE:
  mov $kVirtStart, %ecx
  shr $22, %ecx
  lea (%ebx,%ecx,4), %edx
  lea kPageTab, %eax
  add $kVirtToPhys, %eax
  or  $(PG_SUPERVISOR | PG_READ_WRITE | PG_PRESENT), %eax
  mov %eax, (%edx)

  lea   kPageDir, %eax
  add   $kVirtToPhys, %eax
  mov   %eax, %cr3

  mov   %cr4, %eax
  or    $(CR4_PGE | CR4_PSE), %eax
  mov   %eax, %cr4

  mov   %cr0, %eax
  and   $~(CR0_CD | CR0_NW), %eax // Enable normal caching
  or    $(CR0_PG | CR0_WP), %eax    // Enable paging, write-protection
  mov   %eax, %cr0

 // Load valid linear addresses into the GDTR

  push $kernelGDT
  push $(KERNEL_GDT_LEN << 16)
  lgdt 2(%esp)
  add  $8, %esp

  mov   $KERNEL_DATA_SEL, %eax
  mov   %ax, %ds
  mov   %ax, %es
  mov   %ax, %fs
  mov   %ax, %ss

  ljmp   $KERNEL_CODE_SEL, $reloadCS

reloadCS:
  leave
  ret
*/

printMultibootErrMsg:
  lea  bstrapErrMsg, %eax
  push %eax
  jmp  printBootMsg

printSizeErrMsg:
  lea  sizeErrMsg, %eax
  push %eax
  jmp printBootMsg

printBootMsg:
  pop %esi
  lea VIDEO_RAM_START, %edi
  mov $0x07, %al
  mov $BOOT_DATA_SEL, %bx
  mov %bx, %es

.cpyStr:
  cmpb $0, (%esi)
  je idle

  movsb
  stosb
  jmp .cpyStr

.text

idle:
  hlt
  jmp idle

.section .ddata, "a", @progbits
.align 4

#define MULTIBOOT_MAGIC 0x2BADB002
#define MULTIBOOT_MAGIC2 0x1BADB002

bstrapErrMsg: .asciz "Error: Only Multiboot-compliant loaders are supported."
sizeErrMsg: .asciz "Error: Kernel size is too long!"

/*
.data

EXPORT kCodeSel
  .int KERNEL_CODE_SEL

EXPORT kDataSel
  .int KERNEL_DATA_SEL

EXPORT uCodeSel
  .int USER_CODE_SEL

EXPORT uDataSel
  .int USER_DATA_SEL

EXPORT kTssSel
  .int KERNEL_TSS_SEL

EXPORT kResdTables
  .int KERNEL_RESD_TABLES

EXPORT idtLen
  .int KERNEL_IDT_LEN

EXPORT gdtLen
  .int KERNEL_GDT_LEN

EXPORT tssLen
  .int KERNEL_TSS_LEN


EXPORT initKrnlPDir
  .int kPageDir

EXPORT lowMemPageTable
  .int kLowPageTab

EXPORT k1To1PageTable
  .int k1to1PageTab

EXPORT bootStackTop
  .int kBootStackTop
*/

// Access bits

#define GDT_PRESENT		(1 << 7)
#define GDT_DPL0		(KERNEL_DPL << 5)
#define GDT_DPL3		(USER_DPL << 5)
#define GDT_SYS			0
#define GDT_NON_SYS		(1 << 4)
#define GDT_DATA		0
#define GDT_EXEC		(1 << 3)
#define GDT_EXP_DOWN		(1 << 2)
#define GDT_READABLE		(1 << 1)
#define GDT_WRITABLE		(1 << 1)

// Flag bits

#define GDT_GRAN		(1 << 7)
#define GDT_BITS32		(1 << 6)

#define BOOTSTRAP_BASE		(KERNEL_PHYS_START-KERNEL_VIRT_START)

.data
.type kernelGDT, @object
kernelGDT:
  .quad 0		// Null descriptor

// Kernel Code Descriptor

  .word 0xFFFF	// Limit 0:15
  .word 0x0000	// Base  0:15
  .byte 0x00	// Base 16:23

  // Access
  .byte GDT_READABLE | GDT_EXEC | GDT_NON_SYS | GDT_DPL0 | GDT_PRESENT

  .byte GDT_GRAN | GDT_BITS32 | 0xF   // Limit 16:19 & Flags
  .byte 0	// Base 24:31

// Kernel Data Descriptor

  .word 0xFFFF	// Limit 0:15
  .word 0x0000	// Base  0:15
  .byte 0x00	// Base 16:23

  // Access
  .byte GDT_WRITABLE | GDT_NON_SYS | GDT_DPL0 | GDT_PRESENT

  .byte GDT_GRAN | GDT_BITS32 | 0xF   // Limit 16:19 & Flags
  .byte 0	// Base 24:31

// User Code Descriptor

  .word 0xFFFF	// Limit 0:15
  .word 0x0000	// Base  0:15
  .byte 0x00	// Base 16:23

  // Access
  .byte GDT_READABLE | GDT_EXEC | GDT_NON_SYS | GDT_DPL3 | GDT_PRESENT

  .byte GDT_GRAN | GDT_BITS32 | 0xF   // Limit 16:19 & Flags
  .byte 0	// Base 24:31

// User Data Descriptor

  .word 0xFFFF	// Limit 0:15
  .word 0x0000	// Base  0:15
  .byte 0x00	// Base 16:23

  // Access
  .byte GDT_WRITABLE | GDT_NON_SYS | GDT_DPL3 | GDT_PRESENT

  .byte GDT_GRAN | GDT_BITS32 | 0xF   // Limit 16:19 & Flags
  .byte 0	// Base 24:31

// Bootstrap Code Descriptor

  .word 0xFFFF				// Limit 0:15
  .word BOOTSTRAP_BASE & 0xFFFF		// Base  0:15
  .byte (BOOTSTRAP_BASE >> 16) & 0xFF	// Base 16:23

  // Access
  .byte GDT_READABLE | GDT_EXEC | GDT_NON_SYS | GDT_DPL0 | GDT_PRESENT

  .byte GDT_GRAN | GDT_BITS32 | 0xF	// Limit 16:19 & Flags
  .byte (BOOTSTRAP_BASE >> 24) & 0xFF	// Base 24:31

// Bootstrap Data Descriptor

  .word 0xFFFF				// Limit 0:15
  .word BOOTSTRAP_BASE & 0xFFFF		// Base  0:15
  .byte (BOOTSTRAP_BASE >> 16) & 0xFF	// Base 16:23

  // Access
  .byte GDT_READABLE | GDT_EXEC | GDT_NON_SYS | GDT_DPL0 | GDT_PRESENT

  .byte GDT_GRAN | GDT_BITS32 | 0xF	// Limit 16:19 & Flags
  .byte (BOOTSTRAP_BASE >> 24) & 0xFF	// Base 24:31
