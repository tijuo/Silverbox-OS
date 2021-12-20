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
  mov rcx, rdx
  mov rdx, rdi
  cmp rcx, 0
  je .done
.copy:
  lodsb
  cmp al, 0
  je .set_zero
  stosb
  loop .copy
.set_zero:
  xor rax, rax
  repnz stosb
.done:
  xchg rax, rdx
  ret

kstrlen:
  cld
  mov rsi, rdi
.read_char:
  lodsb
  cmp al, 0
  jne .read_char
  sub rsi, rdi
  dec rsi
  mov rax, rsi
  ret
