[global _start]
[global idle]
[extern main]

%define SYS_EXIT	0

[section .text]

_start:
  call main
  mov  ebx, eax
  mov  eax, SYS_EXIT
  int 0x40

idle:
  jmp idle
