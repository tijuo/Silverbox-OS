#include <asm/asm.h>
#include <asm/grub.h>
#include <asm/kernel.h>

.code32

.extern init
.extern initPaging

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

#define MULTIBOOT_MAGIC 0x2BADB002
#define MULTIBOOT_MAGIC2 0x1BADB002

#define VIDEO_RAM_START	0xB8000

.section .header, "a", @progbits
.align 4

mboot:
        .int MULTIBOOT_HEADER_MAGIC
        .int MULTIBOOT_HEADER_FLAGS
        .int MULTIBOOT_CHECKSUM

        .int 0
        .int 0
        .int 0
        .int 0
        .int 0

	.int MULTIBOOT_EGA_TEXT
	.int MULTIBOOT_EGA_WIDTH
	.int MULTIBOOT_EGA_HEIGHT
	.int MULTIBOOT_EGA_DEPTH

// NOTE: Virtual addresses are used, but paging isn't enabled yet.
// GRUB loads the kernel to its physical address. Memory needs
// to be accessed *extra* carefully here until paging is enabled.

.section .dtext, "ax", @progbits
.align 4

EXPORT start
//  eax has the mulitboot magic value
//  ebx has multiboot data address

  mov	$kBootStackTop, %esp
  mov	%esp, %ebp
  cld

  // Ensure the stack is aligned

  sub $8, %esp
  push %ebp
// Save for main()

  push %ebx

// Check if multiboot magic is valid

  cmp $MULTIBOOT_MAGIC, %eax
  jne .badMboot
  jmp .okMboot

.badMboot:
  jmp printMultibootErrMsg

.okMboot:

  mov $bootGDT, %ecx

// Load Initial GDT

  push %ecx
  push $0x400000	// Push 0x40 limit
  lgdt 2(%esp)
  add  $8, %esp

 // Load valid linear addresses into the GDTR

  mov  $KERNEL_DATA_SEL, %cx
  mov  %cx, %ds
  mov  %cx, %es
  mov  %cx, %ss

  xor  %cx, %cx
  mov  %cx, %fs
  mov  %cx, %gs

  ljmp $KERNEL_CODE_SEL, $.reloadSel

.reloadSel:

  call initPaging
  call init

idle:
  cli
  hlt
  jmp  idle

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

.section .ddata, "a", @progbits

.align 4
bstrapErrMsg: .asciz "Error: Only Multiboot-compliant loaders are supported."

.align 4
sizeErrMsg: .asciz "Error: Kernel size is too long!"

.align 8
bootGDT:
  .quad 0 // Null descriptor
  .quad 0x00CF98000000FFFF // 32-bit code, execute-only, non-conforming
  .quad 0x00CF92000000FFFF // 32-bit data, read-write
