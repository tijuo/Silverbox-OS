#include <asm/asm.h>
#include <asm/grub.h>
#include <asm/kernel.h>

.code32

.extern init
.extern init_paging
.extern kboot_stack_top

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
  cli
//  eax has the mulitboot magic value
//  ebx has multiboot data address

  mov	$kboot_stack_top, %esp
  mov	%esp, %ebp
  cld
  push $0

  // Ensure the stack is aligned

  sub $8, %esp

// Save for main()

  push %ebx

// Check if multiboot magic is valid

  cmp $MULTIBOOT_MAGIC, %eax
  jne .bad_mboot
  jmp .ok_mboot

.bad_mboot:
  jmp print_multiboot_err_msg

.ok_mboot:

  mov $boot_gdt, %ecx

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

  ljmp $KERNEL_CODE_SEL, $.reload_sel

.reload_sel:

  call init_paging
  call init

idle:
  cli
  hlt
  jmp  idle

print_multiboot_err_msg:
  lea  bstrap_err_msg, %eax
  push %eax
  jmp  print_boot_msg

print_size_err_msg:
  lea  size_err_msg, %eax
  push %eax
  jmp print_boot_msg

print_boot_msg:
  pop %esi
  lea VIDEO_RAM_START, %edi
  mov $0x07, %al
  cld

.cpy_str:
  cmpb $0, (%esi)
  je idle

  movsb
  stosb
  jmp .cpy_str

.section .ddata, "a", @progbits

.align 4
bstrap_err_msg: .asciz "Error: Only Multiboot2-compliant loaders are supported."

.align 4
size_err_msg: .asciz "Error: Kernel size is too long!"

.align 8
boot_gdt:
  .quad 0 // Null descriptor
  .quad 0x00CF98000000FFFF // 32-bit code, execute-only, non-conforming
  .quad 0x00CF92000000FFFF // 32-bit data, read-write
