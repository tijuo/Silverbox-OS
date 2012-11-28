%ifndef CONTEXT_H
%define CONTEXT_H

%include "asm/kernel.inc"

[section .data]
IMPORT currentThread
IMPORT tssEsp0
IMPORT schedule
IMPORT tcbTable

IMPORT kernelStack

struc tcb
  .addr_space: resd 1
  .quanta_left: resb 1
  .sched_attrib: resb 1  ; reschedule(1-bit[7]) | priority(3-bits[4:6]) | state(4-bits [0:3])
  ._packing: resw 1
  .queue_head: resd 1
  .queue_tail: resd 1
  .exception_handler: resd 1
  .wait_thread: resd 1
  .signal_handler: resd 1
  .queue_next: resd 1
  .queue_prev: resd 1
  .timer_next: resd 1
  .timer_delta: resd 1

; Execution state

  .state_edi: resd 1
  .state_esi: resd 1
  .state_ebp: resd 1
  .state_esp: resd 1
  .state_ebx: resd 1
  .state_edx: resd 1
  .state_ecx: resd 1
  .state_eax: resd 1

  .state_es: resd 1
  .state_ds: resd 1

  .state_int_num: resd 1
  .state_error_code: resd 1
  .state_eip: resd 1
  .state_cs: resd 1
  .state_eflags: resd 1
  .state_user_esp: resd 1
  .state_user_ss: resd 1
  ._packing2: resd 4
endstruc

%define idleTID		1

%define kStateSize	(7+8)*4
%define uStateSize	(9+8)*4
%define regSize		17*4
%define packing2Size	16
%define TCB_Size	(regSize+44+packing2Size)

%define RUNNING_STATE	4

%define tcbASpaceOff		0
%define tcbQuantaOff		4
%define tcbStateOff		5
%define tcbPriorityOff		5
%define tcbRegsOff		(TCB_Size-regSize-packing2Size)
%define tcbEspOff		(tcbRegsOff+3*4)
%define tcbEdiOff		(tcbRegsOff+0*4)
%define tcbDsOff		(tcbRegsOff+9*4)
%define RESCHEDULE_MASK		0x80
%define STATE_MASK		0x0F
%define DPL_MASK		0x03

%macro SAVE_STATE 0
    push ds
    push es

    pusha

    mov   ax, KERNEL_DATA_SEL
    mov   ds, ax
    mov   es, ax

    test  word [esp+9*4], DPL_MASK	; Was the processor using DPL0 data segments (and probably running in kernel mode)?

    jz   .saveCurrentThread

    lea   esp, [kernelStack + KERNEL_STACK_LEN]
.saveCurrentThread:
    lea   esi, [currentThread]
    push  dword [esi]
%endmacro

; This desperately needs to be cleaned up!

%macro RESTORE_STATE 0
    pop esi 
    mov edi, esi

    ; If the thread's quanta have expired or if the
    ; thread isn't in its RUNNING state, then call the scheduler

    mov  al, [esi+tcbQuantaOff]
    test al, al
    jz   .doSched

    mov  al, [esi+tcbStateOff]
    test al, RESCHEDULE_MASK

    jnz   .doSched
    and	  al, STATE_MASK

    cmp   al, RUNNING_STATE ; RUNNING state
    je    .sameThread

.doSched:
    push  esi
    call  schedule
    add   esp, 4

    mov   edi, eax

.sameThread:
    ; esi - old thread
    ; edi - new thread

    test byte [esi+tcbDsOff], DPL_MASK	; Was the old thread a kernel thread?
    jnz	 .loadNewState

    cmp	 esi, edi			; Are two threads the same?
    je   .popRegs			; If so, just restore the state

    lea  eax, [esi+tcbEdiOff]		; If not, then save the stack pointer to the 
    push dword [eax]			; TCB struct's state structure, while preserving
					; the information already in the structure

    mov  [eax], esp

; Either the old thread was a user thread or it was a kernel thread and the new one is different
.loadNewState:

    test byte [edi+tcbDsOff], DPL_MASK	; Is the new thread a kernel thread?

    jz  .loadKernelState
    lea  esp, [edi+tcbRegsOff]      ; load the new thread's stack

    lea  eax, [esp+uStateSize] ; update the tss' esp0 field to the top of the new stack
    mov  [tssEsp0], eax
;    lea  ebx, [tssEsp0]
;    mov  [ebx], eax

    jmp  .compareAddrSpaces

.loadKernelState:
    lea  eax, [edi+tcbEdiOff]              ; If not, then save the stack pointer to the
    mov  esp, [eax]                     ; TCB struct's state structure, while preserving
                                        ; the information already in the structure
    pop  dword [edi+tcbEdiOff]


.compareAddrSpaces:

    mov  eax, [edi+tcbASpaceOff]
    mov  ebx, [esi+tcbASpaceOff]

    cmp  eax, ebx	   ; do the two threads share the same address space?
    je	.popRegs

    mov  cr3, eax	   ; if not, load the new address space

.popRegs:
    popa

    pop es
    pop ds
%endmacro

%endif
