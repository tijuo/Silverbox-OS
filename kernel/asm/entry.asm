[BITS 32]
%include "asm/grub.inc"
%include "asm/kernel.inc"

[section .header]

IMPORT init

; NOTE: Virtual addresses are used, but paging isn't enabled yet.
; GRUB loads the kernel to it's physical address. Memory needs
; to be accessed *extra* carefully here until paging is enabled.

EXPORT start
;  eax has the mulitboot magic value
;  ebx has multiboot data address
  mov   esp, 0xB000
  mov   ebp, esp

  push  0
  popf

  add ebx, VPhysMemStart

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
;  lea   ecx, [VPhysMemStart + initPageDir + eax] ; Assumes that Kernel is at 0x1000000
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

; 0x0000 - 0x0500 : IVT
; 0x0500 - 0x0D00 : GDT
; 0x0D00 - 0x1000 : IDT
;--- not used ----          ; 0x1000 - 0x3000 : TSS I/O Bitmap
;--- not used ---- ; 0x3000 - 0x4000 : Temporary PDir/PTab
;-- not used ---   ; 0x4000 - 0x5000 : APIC
; 0x1000 - 0x2000 : initial Page directory
; 0x2000 - 0x3000 : The first page table and kernel's page table
; 0x3000 - 0x4000 : Initial server page directory
; 0x4000 - 0x5000 : init server user stack's pde
; 0x5000 - 0x6000 : init server user stack's pte
; 0x6000 - 0x7000 : init server user pde
; 0x7000 - 0x8000 ; idle kernel stack
; 0x8000 - 0x9000 ; kernel stack
; 0x9000 - 0xA000 ; kernel variables
; 0xA000 - 0xB000 : Bootstrap stack

initPaging:
;  push  ebp
;  mov   ebp, esp
                        ; Set the initial page directory at 0x1000
  xor   eax, eax        ; and clear any data left there
  mov   ecx, 1024

  mov   edi, initPageDir

  cld

  rep   stosd

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;  lea   ecx, [initPageDir]
  lea   eax, [initPageDir + 0xFF4]
  mov   edx, 0x2003
;  mov   dword [ecx], edx     ; Map the first page table(0x000000-0x100000)
  mov	dword [eax], edx     ; Map the physical memory range(0xFF400000-0xFF500000)

  lea	edi, [0x2000]
  xor	eax, eax
  mov	ecx, 16

.clearPages:
  stosd
  loop .clearPages

  lea   edi, [0x2000]
  mov   dword [edi], 0x03	; Map 0x0000-0x2000 -> 0xFF400000 - 0xFF401000

  lea	edi, [0x2000+5*4]
  mov	eax, 0x7003
  mov	ecx, 4

.mapFirstTable:
  stosd                    ; Map 0x7000-0xA000 -> 0xFF405000 - 0xFF49000
  add  eax, 0x1000
  loop .mapFirstTable

  mov	eax, 0x10003
  mov	ecx, 240
  lea	edi, [0x2000+16*4]

.mappingLoop:
  stosd
  add	eax, 0x1000
  loop	.mappingLoop

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

; This is redundant
  mov	eax, kPhysStart
  shr	eax, 22
  lea   ecx, [initPageDir + eax] ; Assumes that Kernel is at 0x100000
  mov   edx, 0x2003
  mov   dword [ecx], edx

  mov   ecx, 768
  mov   eax, kPhysStart
  or    eax, 0x03
  lea   edi, [0x2000+256*4]

.mapPhysKernel:
  stosd                    ; 1:1 map kernel memory
  add  eax, 0x1000
  loop .mapPhysKernel

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

  mov	eax, kVirtStart
  shr	eax, 22
  lea   ecx, [initPageDir + eax]
  mov   edx, 0x2003
  mov   dword [ecx], edx        ; Map the Kernel table

  mov   ecx, 768           ; Assumes that the init code is within the 
  mov   eax, kPhysStart    ; first 3MB of the kernel
  or    eax, 0x03
  lea   edi, [0x2000+256*4]

.mapVirtKernel:             ; Map the kernel to address 0xC0000000
  stosd
  add   eax, 0x1000
  loop .mapVirtKernel

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

  lea   ecx, [initPageDir + 4 * 1023]
  mov   edx, initPageDir
  or    edx, 0x03
  mov   dword [ecx], edx        ; Map the page directory into itself

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

  mov   eax, initPageDir   ; Load the page directory into the PDBR
  mov   cr3, eax

  mov   eax, cr0
  or    eax, 0x80000000    ; Enable paging
  mov   cr0, eax

  add	esp, 0xFF400000
  sub	esp, 0x2000

;  leave
  ret

printErrMsg:
  mov esi, bstrapErrMsg
  add esi, kVirtToPhys
  mov edi, 0xB8000
  mov al, 0x07

.cpyStr:
  cmp byte [esi], 0
  jz stop

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
initPageDir equ 0x1000
