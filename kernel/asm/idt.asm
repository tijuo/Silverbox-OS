[BITS 32]
%include "asm/kernel.inc"

IMPORT kernelIDT

[section .text]
; void loadIDT()
; void addIDTEntry( addr, entryNum, attrib )

; kernelIDT[entryNum]

EXPORT addIDTEntry
    push    ebp
    mov	    ebp, esp

    mov     eax, [ebp + 12]
    lea     edx, [eax*8 + kernelIDT]

    mov     eax, [ebp + 8]
    mov     [edx], ax
    shr     eax, 16
    mov     [edx + 6], ax

    mov     eax, [ebp + 16]

    mov     [edx + 2], ax

    mov     byte [edx + 4], 0

    shr     eax, 16
    or      al, 0x80
    mov     [edx + 5], al

    leave
    ret

[section .dtext progbits alloc exec nowrite align=16]

EXPORT loadIDT
	lidt	[idtPointer]
	ret

[section .ddata progbits alloc noexec nowrite align=4]

idtPointer:
    dw    KERNEL_IDT_LEN
    dd    kernelIDT
