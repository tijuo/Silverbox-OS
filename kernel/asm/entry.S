.include "asm/asm.h"
.include "asm/grub.h"
.include "asm/kernel.h"

.code32

.equ PG_PRESENT, 1
.equ PG_READ_WRITE, 2
.equ PG_PWT, 8
.equ PG_USER, 4
.equ PG_CD, 16
.equ PG_READ_ONLY, 0
.equ PG_SUPERVISOR, 0
.equ PG_GLOBAL, 256

.equ CR0_WP, 0x10000
.equ CR0_CD, 0x40000000			// Global cache disable
.equ CR0_NW, 0x20000000			// Disable write-back/write-through (depends on processor)
.equ CR0_PG, 0x80000000

.equ CR4_PGE, 0x80
.equ CR4_PSE, 0x10

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

// NOTE: Virtual addresses are used, but paging isn't enabled yet.
// GRUB loads the kernel to its physical address. Memory needs
// to be accessed *extra* carefully here until paging is enabled.

.section .dtext, "ax", @progbits
.align 16

EXPORT start
//  eax has the mulitboot magic value
//  ebx has multiboot data address
  mov	$kBootStackTop, %esp
  mov	%esp, %ebp
  cld

  push %eax
  push %ebx

  lea kernelGDT, %ecx
  add $kVirtToPhys, %ecx

  push %ecx
  push $(KERNEL_GDT_LEN << 16)
  lgdt 2(%esp)
  add  $8, %esp

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
 // Load valid linear addresses into the GDTR

  cmp $mbootMagic, %eax
  jne .badMboot
  jmp .okMboot

.badMboot:
  jmp printMultibootErrMsg

.okMboot:
  call initPaging2

  call init
  jmp  idle

.equ videoRamStart, 0xB8000
.equ videoBiosStart, 0xC0000

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
  mov $kPageDir, %edi
  rep stosl

  mov $1024, %ecx
  mov $kLowPageTab, %edi
  rep stosl

  mov $1024, %ecx
  mov $kPageTab, %edi
  rep stosl

  mov $1024, %ecx
  mov $k1to1PageTab, %edi
  rep stosl

  mov $kLowPageTab, %ebx
  add $4, %ebx
  mov $0x1000, %ecx		// Skip the first page (leave it as not present to catch NULL dereferences)

.mapLowMemLoop:
  cmp $videoRamStart, %ecx
  jge  .mapVideoRam

  mov %ecx, %eax
  or  $(PG_SUPERVISOR | PG_READ_WRITE | PG_PRESENT), %eax
  mov %eax, (%ebx)

  add $PAGE_SIZE, %ecx
  add $4, %ebx
  jmp .mapLowMemLoop

.mapVideoRam:
  mov $videoRamStart, %ecx
  mov $kLowPageTab, %ebx
  mov $videoRamStart, %edx
  shr $12, %edx
  and $0x3FF, %edx
  lea (%ebx,%edx,4), %ebx

.mapVideoRamLoop:
  cmp $videoBiosStart, %ecx
  jge  .mapKernel

  mov %ecx, %eax
  or  $(PG_USER | PG_READ_WRITE | PG_CD | PG_PWT | PG_PRESENT), %eax
  mov %eax, (%ebx)

  add $PAGE_SIZE, %ecx
  add $4, %ebx
  jmp .mapVideoRamLoop

.mapKernel:
  mov $kPageTab, %ebx
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
  mov $kPageDir, %ebx

  // Map page directory onto itself

  mov $0x3FF, %ecx
  lea (%ebx,%ecx,4), %edx
  mov $kPageDir, %eax
  or  $(PG_SUPERVISOR | PG_READ_WRITE | PG_PRESENT), %eax
  mov %eax, (%edx)

  // Map first page table (1:1 mapping of low memory)

  xor %ecx, %ecx
  lea (%ebx,%ecx,4), %edx
  mov $kLowPageTab, %eax
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
  mov  $k1to1PageTab, %ebx
  jmp  .map1to1Kernel

.tablePresent:
  mov $1, %edi
  mov $kLowPageTab, %ebx

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
  mov  $k1to1PageTab, %eax
  or   $(PG_SUPERVISOR | PG_READ_WRITE | PG_PRESENT), %eax
  mov  %eax, (%edx)

  // Map the kernel's page table

.setKPDE:
  mov $kVirtStart, %ecx
  shr $22, %ecx
  lea (%ebx,%ecx,4), %edx
  mov $kPageTab, %eax
  or  $(PG_SUPERVISOR | PG_READ_WRITE | PG_PRESENT), %eax
  mov %eax, (%edx)

  mov   $kPageDir, %eax
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

  mov   $KERNEL_DATA_SEL, %ax
  mov   %ax, %ds
  mov   %ax, %es
  mov   %ax, %fs
  mov   %ax, %ss

  ljmp   $KERNEL_CODE_SEL, $reloadCS

reloadCS:
  leave
  ret

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
  lea videoRamStart, %edi
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

.equ mbootMagic, 0x2BADB002
.equ mbootMagic2, 0x1BADB002

bstrapErrMsg: .string "Error: Only Multiboot-compliant loaders are supported."
sizeErrMsg: .string "Error: Kernel size is too long!"

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

EXPORT kernelStackTop
  .int kStackTop
