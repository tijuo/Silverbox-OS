[BITS 32]
[section .text]

[global outport8]
[global outport16]
[global outport32]
[global inport8]
[global inport16]
[global inport32]

outport8:
    mov dx, [esp + 4]
    mov ax, [esp + 8]
    out dx, al
    ret

outport16:
    mov dx, [esp + 4]
    mov ax, [esp + 8]
    out dx, ax
    ret

outport32:
    mov dx, [esp + 4]
    mov eax, [esp + 8]
    out dx, eax
    ret

inport8:
    mov dx, [esp + 4]
    in al, dx
    ret

inport16:
    mov dx, [esp + 4]
    in ax, dx
    ret

inport32:
    mov dx, [esp + 4]
    in eax, dx
    ret