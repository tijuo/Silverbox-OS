[global _start]
[extern main]
[extern set_signal_handler]
[extern _default_sig_handler]
[extern sys_set_sig_handler]

%define SYS_EXIT	2

[section .text]
__dummy_sig_handler:
  ret

_start:
  push ebp
  mov  ebp, esp
  push __dummy_sig_handler
  call set_signal_handler
  add  esp, 4

  push dword _default_sig_handler
  call sys_set_sig_handler
  add esp, 4

  call main
  mov  ebx, eax
  mov  eax, SYS_EXIT
  int 0x40
