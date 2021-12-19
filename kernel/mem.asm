[global kmemset]
[global kmemcpy]
[global kstrcpy]
[global kstrncpy]
[global kstrlen]

kmemset:
  cld
  mov rax, rsi
  mov rcx, rdx
  mov rdx, rdi
  rep stosb
  mov rax, rdx
  ret

kmemcpy:
  cld
  mov rcx, rdx
  mov rdx, rdi
  rep movsb
  mov rax, rdx
  ret

kstrcpy:
  cld
  mov rdx, rdi
.copy:
  lodsb
  stosb
  cmp al, 0
  jnz .copy
  mov rax, rdx
  ret

kstrncpy:
  cld
  mov rax, rdi
  mov rcx, rdx
  repnz movsb
  ret

kstrlen:
  cld
  mov rsi, rdi
  xor rcx, rcx
  not rcx
  repnz lodsb
  sub rdi, rsi
  mov rax, rdi
  ret
