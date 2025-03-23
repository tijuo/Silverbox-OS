#include <asm/asm.h>
#include <asm/grub.h>
#include <asm/kernel.h>

.code32

.extern init
.extern init_paging
.extern kboot_stack_top
.extern kphys_start
.extern kvirt_start
.extern kernel_gdt

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

#define VIDEO_RAM_START	0xB8000

// NOTE: Virtual addresses are used, but paging isn't enabled yet.
// GRUB loads the kernel to its physical address. Memory needs
// to be accessed *extra* carefully here until paging is enabled.

.section .dtext, "ax", @progbits
.align 4

EXPORT _start
  cli

//  if loaded via a Multiboot-compliant bootloader,
//  eax has the mulitboot magic value
//  ebx has multiboot data address

// Check if multiboot magic is valid

  cmp $MULTIBOOT_MAGIC, %eax
  jne .bad_mboot

  mov	$kboot_stack_top, %esp
  sub   $kvirt_start, %esp
  add   $kphys_start, %esp

  mov	%esp, %ebp
  cld
  push $0

  // Ensure the stack is aligned

  sub $8, %esp

// Save info struct for main()

  push %ebx

  mov $kernel_gdt, %ecx

  mov $kphys_start, %ebx
  sub $kvirt_start, %ebx
  add %ebx, %ecx

// Load Initial GDT (kernel_gdt is in init/init.c)

  push %ecx
  push $0x400000	// Push 0x40 limit
  lgdt 2(%esp)
  add  $8, %esp

 // Load valid linear addresses into the GDTR

  mov  $BOOT_DATA_SEL, %cx
  mov  %cx, %ds
  mov  %cx, %es

  mov  $KERNEL_DATA_SEL, %cx
  mov  %cx, %ss

  xor  %cx, %cx
  mov  %cx, %fs
  mov  %cx, %gs

  ljmp $BOOT_CODE_SEL, $.reload_sel

.reload_sel:

  call init_paging

// Use kernel virtual addresses for esp and ebp instead of the old physical addresses

  sub %ebx, %esp
  sub %ebx, %ebp

  call init

idle:
  cli
  hlt
  jmp  idle
  ud2

.bad_mboot:
  jmp print_multiboot_err_msg

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
bstrap_err_msg: .asciz "Error: Only Multiboot-compliant loaders are supported."

.align 4
size_err_msg: .asciz "Error: Kernel size is too large!"