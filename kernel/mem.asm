[global kmemset]
[global kmemcpy]
[global kstrcpy]
[global kstrncpy]
[global kstrlen]
[global kstrcmp]
[global kstrncmp]
[global kmemcmp]

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
  test al, al
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

kstrcmp:
  cld
  xor rax, rax

.read_char:
  cmp byte [rdi], 0
  jz .continue

  cmp byte [rsi], 0
  jz .continue

  cmpsb
  je .read_char

.continue:
  mov al, [rdi-1]
  sub al, [rsi-1]
.end:
  ret

kstrncmp:
  cld
  mov rcx, rdx
  xor rax, rax

  test rcx, rcx
  jz .end

.read_char:
  cmp byte [rdi], 0
  jz .continue

  cmp byte [rsi], 0
  jz .continue

  cmpsb
  loope .read_char

.continue:
  mov al, [rdi-1]
  sub al, [rsi-1]
.end:
  ret

kmemcmp:
  cld
  mov rcx, rdx
  xor rax, rax

  test rcx, rcx
  jz .end

  repe cmpsb
  mov al, [rdi-1]
  sub al, [rsi-1]
.end:
  ret
