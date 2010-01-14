[BITS 32]
%include "asm/kernel.inc"

[section .dtext]

; void addGDTEntry( word sel, addr_t base, dword limit, dword flags )
; void loadGDT()

EXPORT loadGDT
    lgdt    [gdtPointer]

    mov     ax, tssSel
    ltr     ax

    mov	    ax, 0
    lldt    ax

    mov     ax, kDataSel
    mov     ds, ax
    mov     es, ax
    mov     ss, ax

    jmp     kCodeSel:loadCs

loadCs:
    ret

[section .ddata]

gdtPointer:
    dw    0x298
    dd    _kernelGDT

;EXPORT tssEntry
;        dw 0, 0
;EXPORT tssEsp
;        dd 0
;        dw kDataSel, 0

;        dd 0
;        dw 0, 0

;        dd 0
;        dw 0, 0

;        dd 0
;        dd 0, 0
;        dd 0, 0, 0, 0
;        dd 0, 0, 0, 0
;        dw 0, 0
;        dw 0, 0
;        dw 0, 0
;        dw kDataSel, 0
;        dw 0, 0
;        dw 0, 0
;        dw 0, 0
;        dw 0, 104

;EXPORT tssIOBitmap
;%rep	32		  ; Map ports 0h - 3FFh
;	dd 0xFFFFFFFF ; (8 bits/byte * 4 bytes/dword * 32 dwords = 1024)
;%endrep			  ; the rest of the bits are automatically set as 1
