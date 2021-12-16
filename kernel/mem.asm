[global memset]
[global memcpy]
[global strcpy]
[global strncpy]

memset:
  cld
  mov rax, rsi
  mov rcx, rdx
  mov rdx, rdi
  rep stosb
  mov rax, rdx
  ret

memcpy:
  cld
  mov rcx, rdx
  mov rdx, rdi
  rep movsb
  mov rax, rdx
  ret

strcpy:
  cld
  mov rax, rdi
  repnz movsb
  ret

strncpy:
  cld
  mov rax, rdi
  mov rcx, rdx
  repnz movsb
  ret
