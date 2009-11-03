[section .text]

[extern exit]
[extern send]
[extern sleep]
[global _start]

atomicInc:
    push    ebp
    mov     ebp, esp

    mov     eax, [ebp + 8]
    lock    inc  dword [eax]

    leave
    ret

atomicDec:
    push    ebp
    mov     ebp, esp

    mov     eax, [ebp + 8]
    lock    dec  dword [eax]

    leave
    ret

testAndSet:
    push    ebp
    mov     ebp, esp

    mov     eax, [ebp+12]
    mov     ecx, [ebp+8]

    lock    xchg    eax, [ecx]

    leave
    ret

printChar:
    push ebp
    mov  ebp, esp

    push dword 1
    push letter
    push 0xA1
    push 5

    call send

    add esp, 32

    leave
    ret

_start:
    mov  ebp, esp

    push 1000
    call sleep
    add esp, 4

    push resource

looop:

    call printChar

    call atomicDec

    cmp dword [resource], 0
    jz .exit
    jmp looop

.exit
    push 0

    call exit

[section .data]

resource: dd 10
letter: db 'a'
