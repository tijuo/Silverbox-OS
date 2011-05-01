[BITS 32]
%include "asm/grub.inc"
%include "asm/kernel.inc"

[section .header]

IMPORT init
IMPORT commandLine

; NOTE: Virtual addresses are used, but paging isn't enabled yet.
; GRUB loads the kernel to it's physical address. Memory needs
; to be accessed *extra* carefully here until paging is enabled.

EXPORT start
;  eax has the mulitboot magic value
;  ebx has multiboot data address
  lea   esp, [_kernelBootStack + 4096]
  mov   ebp, esp

  push  0
  popf

  lea ebx, [ebx+VPhysMemStart]

  push ebx

  cmp eax, mbootMagic
  je  okMagic

  mov eax, printErrMsg
  add eax, kVirtToPhys
  call eax

okMagic:

  mov   eax, initPaging
  add   eax, kVirtToPhys
  call  eax

;  add	esp, VPhysMemStart

;  mov eax, kPhysStart
;  shr   eax, 22
;  lea   ecx, [VPhysMemStart + _initKrnlPDIR + eax] ; Assumes that Kernel is at 0x1000000
;  xor   edx, edx
;  mov   dword [ecx], edx     ; Unmap the third page table

  mov	eax, init
  call   eax

IMPORT kCode
IMPORT kBss
IMPORT kEnd

ALIGN 4
mboot:
        dd MULTIBOOT_HEADER_MAGIC
        dd MULTIBOOT_HEADER_FLAGS
        dd MULTIBOOT_CHECKSUM

        dd mboot
        dd kCode
        dd kBss
        dd kEnd
        dd start

[section .dtext]

; Physical Memory Map

; 0x0000  - 0x04FF : IVT
; 0x88000 - 0x887FF : IDT
; 0x88800 - 0x88F97 : GDT
; 0x88F98 - 0x88FFF : TSS
; 0x89000 - 0x8AFFF : TSS IO bitmap
; 0x8B000 - 0x8BFFF : initial Page directory
; 0x8C000 - 0x8CFFF : The kernel's page table
; 0x8D000 - 0x8DFFF : Initial server page directory
; 0x8E000 - 0x8EFFF : init server user stack's page table
; 0x8F000 - 0x8FFFF : init server user stack's page
; 0x90000 - 0x90FFF : init server user page table
; 0x91000 - 0x91FFF : idle kernel stack
; 0x92000 - 0x92FFF : kernel stack
; 0x93000 - 0x93FFF : kernel variables
; 0x94000 - 0x94FFF : the idle thread's first page table
; 0x95000 - 0x95FFF : The second kernel page table
; 0x96000 - 0x96FFF : The initial server's first page table
; 0x97000 - 0x97FFF : Bootstrap stack

initPaging:
;  push  ebp
;  mov   ebp, esp
                        ; Set the initial page directory at 0x1000
                        ; and clear any data left there

  push _kernelIDT
  push _initKrnlPDIR
  push _firstKrnlPTAB
  push _initServStackPTAB
  push _initServPTAB
  push _kernelVars
  push _firstPTAB
  push _secondKrnlPTAB
  push _initFirstPTAB

  mov eax, 9

  mov ebx, wipePages
  add ebx, kVirtToPhys
  call ebx

  add esp, 36

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

  lea	eax, [_initKrnlPDIR + 0xFF8]
  mov	edx, _secondKrnlPTAB
  or	edx, 3
  mov	dword [eax], edx

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

  mov	ecx, _initKrnlPDIR
  mov   edx, _firstPTAB
  or	edx, 7
  mov   dword [ecx], edx     ; 1:1 map the physical memory range (0x00-0xBFFFF)

  mov   edi, _firstPTAB
  mov	eax, 0x03
  mov	ecx, 160

; Map conventional memory

.mapFirstTable1:
  stosd
  add  eax, 0x1000
  loop .mapFirstTable1

; Map video memory (no-cache, write-through, user)

  mov	eax, 0xA001F
  mov	ecx, 32
  lea	edi, [_firstPTAB+160*4]

.mapFirstTable2:
  stosd
  add	eax, 0x1000
  loop	.mapFirstTable2

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

  mov	eax, kPhysStart
  shr	eax, 20		; page directory offset
  lea   ecx, [_initKrnlPDIR + eax] ; Assumes that Kernel is at 0x1000000
  mov   edx, _firstKrnlPTAB	; base address
  or    edx, 3			; supervisor, rw, present
  mov   dword [ecx], edx

  mov   ecx, 1024
  mov   eax, kPhysStart
  or    eax, 0x03
  mov   edi, _firstKrnlPTAB

.mapPhysKernel:
  stosd                    ; 1:1 map kernel memory
  add  eax, 0x1000
  loop .mapPhysKernel

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

  mov	eax, kVirtStart
  shr	eax, 20
  lea   ecx, [_initKrnlPDIR + eax]
  mov   edx, _firstKrnlPTAB
  or    edx, 3
  mov   dword [ecx], edx        ; Map the Kernel table

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

  lea   ecx, [_initKrnlPDIR + 4 * 1023]
  mov   edx, _initKrnlPDIR
  or    edx, 0x03
  mov   dword [ecx], edx        ; Map the page directory into itself

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

  mov   eax, _initKrnlPDIR   ; Load the page directory into the PDBR
  mov   cr3, eax

  mov   eax, cr0
  or    eax, 0x80000000    ; Enable paging
  mov   cr0, eax

  add	esp, VPhysMemStart

;  leave
  ret

; eax - the number of page directories to wipe (pages are on the stack)

wipePages:
  push ebp
  mov ebp, esp

  cld

  mov ecx, eax

  add	esp, 8

.startWipe:
  pop edi
;  mov edi, [esp]
;  add esp, 4

  push ecx

  xor   eax, eax
  mov   ecx, 1024

  rep   stosd

  pop  ecx
  loop .startWipe

.endWipe:
  leave
  ret

printErrMsg:
  mov esi, bstrapErrMsg
  add esi, kVirtToPhys
  mov edi, 0xB8000
  mov al, 0x07

.cpyStr:
  cmp byte [esi], 0
  je stop

  movsb
  stosb
  jmp .cpyStr

stop:
  hlt
  jmp stop


[section .ddata]

;GRUBBootInfo: dd 0
mbootMagic equ 0x2BADB002
bstrapErrMsg: db 'Error: Only Multiboot-compliant loaders are supported.',0
