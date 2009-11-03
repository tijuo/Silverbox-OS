%ifndef CONTEXT_H
%define CONTEXT_H

%include "asm/kernel.inc"

[section .data]
IMPORT currentThread
IMPORT tssEsp0
IMPORT schedule
IMPORT kVirtStart
IMPORT tcbTable

idleStack   equ VPhysMemStart + 0x6000
kernelStack equ VPhysMemStart + 0x7000

idleTID	    equ 0

TCB_Size    equ 0x5c
regSize	    equ 0x40

aSpaceOff   equ 0
quantaOff   equ 4
stateOff    equ 5
priorityOff equ 5
regOff      equ 24

; FIXME: This needs an entire overhaul

%macro SAVE_STATE 0
    o16 push ds
    o16 push es

    pusha

    mov   ax, kDataSel
    mov   ds, ax
    mov   es, ax

     lea   esp, [kernelStack]
     push  dword [currentThread]
%endmacro

; This desperately needs to be cleaned up!

%macro RESTORE_STATE 0
    pop esi 

    ; If the thread's quanta have expired or if the
    ; thread isn't in its RUNNING state, then call the scheduler

    cmp   byte [esi+quantaOff], 0
    jle   .doSched

    mov	  al, [esi+stateOff]
    and	  al, 0x0F 

    cmp   byte al, 4 ; RUNNING state
    jne  .doSched

    mov  edi, esi

    cmp  edi, [tcbTable]
    jne	 .loadStack
    mov	 esp, idleStack - regSize
    jmp  .pop_regs
.loadStack:
    lea  esp, [edi+regOff]
    jmp  .pop_regs

.doSched:
    push  esi
    call  schedule
    add   esp, 4
    mov   edi, eax

    cmp	 edi, [tcbTable+idleTID*TCB_Size]
    jne  .load_stack
    mov  esp, idleStack - regSize
    jmp  .compare_threads

.load_stack:
    lea  esp, [edi+regOff]	; load the new thread's stack

.compare_threads:

    cmp esi, edi
    jz  .pop_regs

    lea  eax, [esp+regSize] ; update the tss' esp0 field to the top of the new stack
    mov  ebx, [tssEsp0]
    mov  [ebx], eax

    mov  edi, [edi+aSpaceOff]
    mov  cr3, edi

.pop_regs:
    popa

    o16 pop es
    o16 pop ds
%endmacro

%endif
