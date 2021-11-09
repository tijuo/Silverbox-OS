.ifndef CONTEXT_H
.set CONTEXT_H, 1

#include <asm/kernel.h>

IMPORT switchStacks

.equ DPL_MASK,		0x03

.macro SAVE_STATE
    push %eax
    push %ecx
    ECODE_SAVE_STATE
.endm

// Used to streamline saving stack state with error code pushed on

.macro ECODE_SAVE_STATE
    push %edx
    push %ebx
    push %ebp
    push %esi
    push %edi

    mov   $KERNEL_DATA_SEL, %eax
    mov   %eax, %ds
    mov   %eax, %es

    push  %esp		// Pass the stack pointer to the handler
.endm

.macro RESTORE_STATE
    call switchStacks
    add  $4, %esp

//   Load the user code/data segments if switching back to user mode

    testw $DPL_MASK, 36(%esp)
    jz  1f

    mov   $USER_DATA_SEL, %ax
    mov   %ax, %ds
    mov   %ax, %es

1:
    pop %edi
    pop %esi
    pop %ebp
    pop %ebx
    pop %edx
    pop %ecx
    pop %eax
.endm

.endif
