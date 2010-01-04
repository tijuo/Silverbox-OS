[BITS 32]

%include "asm/context.inc"

[section .text]

IMPORT handleCPUException
IMPORT timerInt
IMPORT _syscall
IMPORT handleIRQ

enterExceptionHandler:
    SAVE_STATE
    call handleCPUException
    RESTORE_STATE
    add  esp, 8
    iret

EXPORT intHandler0
	push dword	0
	push dword	0
	jmp     enterExceptionHandler

EXPORT intHandler1
	push dword	0
	push dword	1
	jmp     enterExceptionHandler

EXPORT intHandler2
	push dword	0
	push dword	2
	jmp     enterExceptionHandler

EXPORT intHandler3
	push dword	0
	push dword	3
	jmp     enterExceptionHandler

EXPORT intHandler4
    push dword    0
    push dword    4
    jmp     enterExceptionHandler

EXPORT intHandler5
    push dword    0
    push dword    5
    jmp     enterExceptionHandler

EXPORT intHandler6
    push dword    0
    push dword    6
    jmp     enterExceptionHandler

EXPORT intHandler7
    push dword    0
    push dword    7
	jmp     enterExceptionHandler

EXPORT intHandler8
    push dword    8
	jmp     enterExceptionHandler

EXPORT intHandler9
    push dword    0
    push dword    9
	jmp     enterExceptionHandler

EXPORT intHandler10
    push dword    10
	jmp     enterExceptionHandler

EXPORT intHandler11
    push dword    11
	jmp     enterExceptionHandler

EXPORT intHandler12
    push dword    12
	jmp     enterExceptionHandler

EXPORT intHandler13
    push dword   	13
	jmp     enterExceptionHandler

EXPORT intHandler14
    push dword    14
	jmp     enterExceptionHandler

EXPORT intHandler16

    push dword    0
    push dword    16
	jmp     enterExceptionHandler

EXPORT intHandler17
    push dword    17
	jmp     enterExceptionHandler

EXPORT intHandler18

    push dword    0
    push dword    18
	jmp     enterExceptionHandler

EXPORT intHandler19
    push dword    0
    push dword    19
	jmp     enterExceptionHandler

EXPORT invalidIntHandler
	push dword	0
	push dword	0xBAD1
    jmp     enterExceptionHandler

EXPORT spuriousIntHandler
	push dword	0
	push dword	39
    jmp     enterExceptionHandler

enterIrqHandler:
    SAVE_STATE

    call handleIRQ

    RESTORE_STATE
    add esp, 8

    iret

;EXPORT irq0Handler
;        mov     dword [kernelStack-4], 0x20

;    push dword 0
;    push dword 0x20
;    jmp enterIrqHandler

EXPORT irq1Handler
    push dword 0
    push dword 0x21
    jmp enterIrqHandler

EXPORT irq2Handler
    push dword 0
    push dword 0x22
    jmp enterIrqHandler

EXPORT irq3Handler
    push dword 0
    push dword 0x23
    jmp enterIrqHandler

EXPORT irq4Handler
    push dword 0
    push dword 0x24
    jmp enterIrqHandler

EXPORT irq5Handler
    push dword 0
    push dword 0x25
    jmp enterIrqHandler

EXPORT irq6Handler
    push dword 0
    push dword 0x26
    jmp enterIrqHandler

EXPORT irq7Handler
    push dword 0
    push dword 0x27
    jmp enterIrqHandler

EXPORT irq8Handler
    push dword 0
    push dword 0x28
    jmp enterIrqHandler

EXPORT irq9Handler
    push dword 0
    push dword 0x29
    jmp enterIrqHandler

EXPORT irq10Handler
    push dword 0
    push dword 0x2A
    jmp enterIrqHandler

EXPORT irq11Handler
    push dword 0
    push dword 0x2B
    jmp enterIrqHandler

EXPORT irq12Handler
    push dword 0
    push dword 0x2C
    jmp enterIrqHandler

EXPORT irq13Handler
    push dword 0
    push dword 0x2D
    jmp enterIrqHandler

EXPORT irq14Handler
    push dword 0
    push dword 0x2E
    jmp enterIrqHandler

EXPORT irq15Handler
    push dword 0
    push dword 0x2F
    jmp enterIrqHandler

EXPORT timerHandler
    push dword 0
    push dword 0x20
    SAVE_STATE
    call timerInt
    RESTORE_STATE
	add  esp, 8
    iret

EXPORT syscallHandler
    push dword 0
    push dword 0x40
    SAVE_STATE

    call _syscall

    RESTORE_STATE
    add  esp, 8

    iret
