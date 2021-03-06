.include "asm/asm.h"

.code32
.text

EXPORT ioWait
    xor %eax, %eax

    jmp 1f

1:
    test %eax, %eax
    jnz 2f
    inc %eax
    jmp 1b

2:
    ret

EXPORT inByte
  mov 4(%esp), %dx
  in %dx, %al
  and $0xFF, %eax

  ret

EXPORT inWord
  mov 4(%esp), %dx

  in %dx, %ax
  and $0xFFFF, %eax

  ret

EXPORT inDword
  mov 4(%esp), %dx
  in  %dx, %eax

  ret

EXPORT outByte
  mov 4(%esp), %dx
  mov 8(%esp), %al
  out %al, %dx

  ret

EXPORT outWord
  mov 4(%esp), %dx
  mov 8(%esp), %ax
  out %ax, %dx

  ret

EXPORT outDword
  mov 4(%esp), %dx
  mov 8(%esp), %eax
  out %eax, %dx

  ret
