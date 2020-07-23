%ifndef CONTEXT_H
%define CONTEXT_H

%include "asm/kernel.inc"

[section .code]
IMPORT switchStacks

%define DPL_MASK		0x03

%macro SAVE_STATE 0
    push eax
    push ecx
    ECODE_SAVE_STATE
%endmacro

; Used to streamline saving stack state with error code pushed on

%macro ECODE_SAVE_STATE 0
    push edx
    push ebx
    push ebp
    push esi
    push edi

    mov   ax, KERNEL_DATA_SEL
    mov   ds, ax
    mov   es, ax

    push  esp		; Pass the stack pointer to the handler
%endmacro

%macro RESTORE_STATE 0
    call switchStacks
    add  esp, 4

;   Load the user code/data segments if switching back to user mode

    test word [esp+4*9], DPL_MASK
    jz  .popRegs

    mov   ax, USER_DATA_SEL
    mov   ds, ax
    mov   es, ax

.popRegs:
    pop edi
    pop esi
    pop ebp
    pop ebx
    pop edx
    pop ecx
    pop eax
%endmacro

%endif
