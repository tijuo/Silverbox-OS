[BITS 32]
%include "asm/asm.inc"

EXPORT memset
  mov  edi, [esp+4]
  mov  al,  [esp+8]
  mov  ah, al
  movzx ebx, ax
  shl  eax, 16
  or   eax, ebx
  mov  ecx, [esp+12]

.setPreBytes:
  test ecx, ecx
  jz  .end
  test edi, 0x3
  jz .setDwords
  stosb
  dec ecx
  jmp .setPreBytes

.setDwords:
  mov edx, ecx
  and edx, 0x3
  shr ecx, 2

  rep stosd

  mov ecx, edx
  rep stosb

.end:
  mov eax, [esp+4]
  ret

EXPORT memcpy
  mov esi, [esp+8]
  mov edi, [esp+4]
  mov ecx, [esp+12]
  mov edx, ecx
  and edx, 0x3
  shr ecx, 2

  rep movsd

  mov ecx, edx
  rep movsb

  mov eax, [esp+4]
  ret
