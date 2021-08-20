.globl _start
.extern main

.set SYS_EXIT,	0
.set SYSCALL_INT, 0x40

.text

_start:
  call main
  mov  %eax, %ebx
  mov  $SYS_EXIT, %eax
  int  $SYSCALL_INT
