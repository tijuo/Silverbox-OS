.code32
.include "asm/kernel.h"

IMPORT kernelIDT

.text

// void loadIDT()
// void addIDTEntry( addr, entryNum, attrib )

// kernelIDT[entryNum]

EXPORT addIDTEntry
    push    %ebp
    mov	    %esp, %ebp

    push    %ebx

    mov     12(%ebp), %eax
    lea     kernelIDT, %ebx
    lea     (%ebx, %eax, 8), %edx // edx, [eax*8 + kernelIDT]

    mov     8(%ebp), %eax
    mov     %ax, (%edx)
    shr     $16, %eax
    mov     %ax, 6(%edx)

    mov     16(%ebp), %eax

    mov     %ax, 2(%edx)

    movb    $0, 4(%edx)

    shr     $16, %eax
    or      $0x80, %al
    mov     %al, 5(%edx)

    pop    %ebx

    leave
    ret

.section .dtext, "ax", @progbits
.align 16

EXPORT loadIDT
	lidt	idtPointer
	ret

.section .ddata, "a", @progbits
.align 4

idtPointer:
    .word    KERNEL_IDT_LEN
    .int     kernelIDT
