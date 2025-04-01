.globl _start
.extern main

.text

_start:
  call main
  mov  %eax, %ebx
  hlt
