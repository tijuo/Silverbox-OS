%ifndef CONTEXT_H
%define CONTEXT_H

%include "asm/kernel.inc"

[section .code]
IMPORT schedule
IMPORT updateCurrentThread

[section .data]
IMPORT currentThread
IMPORT tssEsp0

%define stateCsOff		(8*4)
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

  ; Was the processor using a DPL0 code segment
  ; (thus running in kernel mode)?

    test  word [esp+stateCsOff], DPL_MASK

    jz   .saveCurrentThread

    lea   esp, [kernelStackTop]
.saveCurrentThread:
    lea   esi, [currentThread]
    push  dword [esi]
%endmacro

%macro RESTORE_STATE 0
    call updateCurrentThread
    add  esp, 4

    test eax, eax
    jz   .testDPL

;  Load the new stack that was returned by updateCurrentThread

    mov  esp, eax

.testDPL:

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
