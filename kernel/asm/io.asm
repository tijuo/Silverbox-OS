%include "asm/asm.inc"

EXPORT ioWait
  xor  eax, eax

  jmp .1
.1:
  test eax, eax
  jnz .2
  inc eax
  jmp .1
.2:
  ret

EXPORT inByte
  mov  dx, [esp+4]
  in   al, dx

  and  eax, 0xFF
  ret

EXPORT inWord
  mov  dx, [esp+4]
  in   ax, dx

  and  eax, 0xFFFF
  ret

EXPORT inDword
  mov  dx, [esp+4]
  in   eax, dx

  ret

EXPORT outByte
  mov   dx, [esp+4]
  mov   al, [esp+8]
  out   dx, al

  ret

EXPORT outWord
  mov  dx, [esp+4]
  mov  ax, [esp+8]
  out  dx, ax

  ret

EXPORT outDword
  mov  dx, [esp+4]
  mov  eax, [esp+8]
  out  dx, eax

  ret
