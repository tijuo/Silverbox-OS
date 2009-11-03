[BITS 32]
%include "asm/kernel.inc"

[section .text]

IMPORT tssEsp0

; Bit 7 of CR4 enables global pages


; void atomicInc( int32 *value )
; void atomicDec( int32 *value )
; int testAndSet( int32 *lock, int32 value )

; byte inByte( word port )
; word inWord( word port )
; dword inDword( word port )
; void outByte( word port, byte data )
; void outWord( word port, word data )
; void outDword( word port, dword data )

; void setCR0( dword value )
; void setCR3( dword value )
; void setCR4( dword value )
; void setEflags( dword value )
; dword getCR0()
; dword getCR2()
; dword getCR3()
; dword getCR4()
; dword getEflags()

; bool intIsEnabled()

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Atomic operations
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;EXPORT atomicInc
;    push    ebp
;    mov     ebp, esp

;    mov     eax, [ebp + 8]
;    lock    inc  dword [eax]

;    leave
;    ret

;EXPORT atomicDec
;    push    ebp
;    mov     ebp, esp

;    mov     eax, [ebp + 8]
;    lock    dec  dword [eax]

;    leave
;    ret

;EXPORT testAndSet
;    push    ebp
;    mov     ebp, esp

;    mov     eax, [ebp+12]
;    mov     ecx, [ebp+8]

;    lock    xchg    eax, [ecx]

;    leave
;    ret

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Control register operations
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

EXPORT setCR0
    push    ebp
    mov     ebp, esp

    mov     eax, [esp + 8]
    mov     cr0, eax

    leave
    ret

EXPORT setCR3
    push    ebp
    mov     ebp, esp

    mov     eax, [esp + 8]
    mov     cr3, eax

    leave
    ret

EXPORT setCR4
    push    ebp
    mov     ebp, esp

    mov     eax, [esp + 8]
    mov     cr4, eax

    leave
    ret

EXPORT getCR0
    mov     eax, cr0
    ret

EXPORT getCR2
    mov     eax, cr2
    ret

EXPORT getCR3
    mov     eax, cr3
    ret

EXPORT getCR4
    mov     eax, cr4
    ret

EXPORT getEflags
    push    ebp
    mov     ebp, esp

    pushf
    pop     eax

    leave
    ret

EXPORT setEflags
    push    ebp
    mov     ebp, esp

    push dword [ebp+8]
    popf

    leave
    ret

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Interrupt related operations
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

EXPORT intIsEnabled
    push    ebp
    mov     ebp, esp

    call    getEflags

    shr     eax, 8
    and     eax, 1

    leave
    ret

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Context related operations
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

; XXX: Need to redo this

; void saveAndSwitchContext( TCB *oldThread, TCB *newThread )

;EXPORT saveAndSwitchContext
;    pushf
;    cli
;    push    dword kCodeSel
;    push    dword [esp+8] ; Push the return address for the eip
;    push    dword 0
;    push    dword 0xcafebabe
;    push    dword kDataSel
;    push    dword kDataSel
;    pusha
;    mov     eax, [esp+64]
;    mov     [eax], esp	   ; Save the old thread's stack

;    mov     ecx, [esp+68]  ; Compare the two address spaces
;    mov     ecx, [ecx+4]   ; if they're the same, don't load
;    mov     edx, cr3       ; cr3 to avoid a TLB flush
;    cmp     ecx, edx
;    jz      ._skip

;    mov     cr3, ecx

;._skip:
;    mov     ecx, 0x175
;    mov     eax, [esp+68]
;    mov     eax, [eax]

;    mov     esp, eax

;    add     eax, 0x44
;    mov     ebx, [tssEsp0]
;    mov     [ebx], eax

;    popa
;    pop     es
;    pop     ds
;    add     esp, 0x08
;    iret
