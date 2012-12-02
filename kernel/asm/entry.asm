[BITS 32]
%include "asm/grub.inc"
%include "asm/kernel.inc"

%define PG_PRESENT	(1 << 0)
%define PG_READ_WRITE	(1 << 1)
%define PG_READ_ONLY	0
%define PG_SUPERVISOR	0
%define PG_USER		(1 << 2)
%define PG_GLOBAL	(1 << 8)
%define PG_CD 		(1 << 4)
%define PG_PWT		(1 << 3)

%define CR0_WP		(1 << 16)
%define CR0_CD		(1 << 30)	; Global cache disable
%define CR0_NW		(1 << 29)	; Disable write-back/write-through (depends on processor)
%define CR0_PG		(1 << 31)

%define CR4_PGE		(1 << 7)
%define CR4_PSE		(1 << 4)

[section .header progbits alloc noexec nowrite align=4]

IMPORT init
IMPORT initKrnlPDir
IMPORT kernelBootStack
IMPORT lowMemPTab
IMPORT kernelGDT

mboot:
        dd MULTIBOOT_HEADER_MAGIC
        dd MULTIBOOT_HEADER_FLAGS
        dd MULTIBOOT_CHECKSUM

        dd mboot
        dd kCode
        dd kBss
        dd kEnd
        dd start

; NOTE: Virtual addresses are used, but paging isn't enabled yet.
; GRUB loads the kernel to it's physical address. Memory needs
; to be accessed *extra* carefully here until paging is enabled.

[section .dtext progbits alloc exec nowrite align=16]

IMPORT free_page_ptr

EXPORT start
;  eax has the mulitboot magic value
;  ebx has multiboot data address
  lea   esp, [kernelBootStack + KERNEL_BOOT_STACK_LEN]
  add   esp, kVirtToPhys

  cld

  push ebx

  lea ecx, [kernelGDT]
  add ecx, kVirtToPhys

  push ecx
  push KERNEL_GDT_LEN << 16
  lgdt [esp+2]
  add  esp, 8

  mov  cx, BOOT_DATA_SEL
  mov  ds, cx
  mov  es, cx
  mov  ss, cx
  mov  cx, KERNEL_DATA_SEL
  mov  fs, cx
  xor  cx, cx
  mov  gs, cx

  sub esp, kVirtToPhys
  jmp BOOT_CODE_SEL:.reloadSel

.reloadSel:
  push kernelGDT
  push KERNEL_GDT_LEN << 16
  lgdt [esp+2]
  add  esp, 8

  cmp eax, mbootMagic
  jne  .badMboot
  jmp .okMboot

.badMboot:
  jmp printMultibootErrMsg

.okMboot:
  call initPaging

  xor ebp, ebp
  call   init

initPaging:

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

; Map the first page table

  mov	ebp, [initKrnlPDir]
  mov   edi, lowMemPTab
  lea	edx, [edi+kVirtToPhys]

  or	edx, PG_USER | PG_READ_WRITE | PG_PRESENT
  mov   dword [fs:ebp], edx

; 1:1 map the physical memory range (0x00-0xA00000)

  mov	eax, 0x00 | PG_SUPERVISOR | PG_READ_WRITE | PG_PRESENT
  mov	ecx, 0xA0000 / 4096

.mapFirstTable1:
  stosd
  add  eax, 0x1000
  loop .mapFirstTable1

; Map video memory (no-cache, write-through, user, r/w)

  mov	eax, 0xA0000 | PG_CD | PG_PWT | PG_USER | PG_READ_WRITE | PG_PRESENT
  mov	ecx, (128*1024) / 4096
  lea	edi, [lowMemPTab+(0xA0000/4096)*4]

.mapFirstTable2:
  stosd
  add	eax, 0x1000
  loop	.mapFirstTable2

  ; Set the unused entries as not-present

  lea  edi, [lowMemPTab+(0xC0000/4096)*4]
  mov  ecx, (0x100000-0xC0000) / 4096
  xor  eax, eax

  rep stosd

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
  mov   esi, kSize
  cmp   esi, 256*1048576-KERNEL_RESD_TABLES*4096*1024
  jg	printSizeErrMsg

  shr   esi, 12
  xor	ebx, ebx
  lea   edx, [free_page_ptr]
  mov   edx, [edx]

.mapKernel:
  lea	edi, [edx+kPhysToVirt]

  cmp   esi, 1024
  jge   .continueMapping

  lea   edi, [edi+4*esi]	; Clear unused PTEs
  mov	ecx, 1024
  sub	ecx, esi
  xor   eax, eax

  rep   stosd
  lea   edi, [edx+kPhysToVirt]

.continueMapping:
  mov   eax, kPhysStart
  or    eax, PG_SUPERVISOR | PG_GLOBAL | PG_READ_WRITE | PG_PRESENT

  cmp   esi, 1024
  jge    .setCounters
  mov	ecx, esi
  xor	esi, esi
  jmp   .mapKernelTable

.setCounters:
  mov   ecx, 1024
  sub	esi, ecx

.mapKernelTable:
  stosd
  add  eax, 0x1000
  loop .mapKernelTable

  ; Set the PDEs

  lea	eax, [kVirtStart+ebx]
  lea   ecx, [kPhysStart+ebx]
  shr	eax, 20		; page directory offset
  shr   ecx, 20

  lea   eax, [ebp + eax]
  lea   ecx, [ebp + ecx]
  mov   edi, edx
  or    edi, PG_SUPERVISOR | PG_READ_WRITE | PG_PRESENT
  mov   [fs:eax], edi
  mov   [fs:ecx], edi

  add  ebx, 4096*1024
  add  edx, 4096
  test esi, esi
  jnz .mapKernel

  lea edi, [free_page_ptr]
  mov [edi], edx

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

  lea   ecx, [ebp + 4 * 1023]
  mov   edx, ebp
;  add	edx, kVirtToPhys
  or    edx, PG_SUPERVISOR | PG_READ_WRITE | PG_PRESENT
  mov   dword [fs:ecx], edx        ; Map the page directory into itself

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;  mov   eax, initKrnlPDir   ; Load the page directory into the PDBR
;  add   eax, kVirtToPhys
;  mov   cr3, eax
  mov	cr3, ebp

  mov	eax, cr4
  or	eax, CR4_PGE
  mov	cr4, eax

  mov   eax, cr0
  and	eax, ~(CR0_CD | CR0_NW)	; Enable normal caching
  or    eax, CR0_PG | CR0_WP    ; Enable paging, write-protection
  mov   cr0, eax

  mov	ax, KERNEL_DATA_SEL
  mov	ds, ax
  mov	es, ax
  mov	ss, ax

  jmp   KERNEL_CODE_SEL:.reloadCS
;  push kCodeSel
;  push .reloadCS
;  retf

.reloadCS:
  ret

printMultibootErrMsg:
  lea  eax, [bstrapErrMsg]
  push eax
  jmp  printBootMsg

printSizeErrMsg:
  lea  eax, [sizeErrMsg]
  push eax
  jmp printBootMsg

printBootMsg:
  pop esi
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

[section .ddata progbits alloc noexec nowrite align=4]

;GRUBBootInfo: dd 0
mbootMagic equ 0x2BADB002
mbootMagic2 equ 0x1BADB002
bstrapErrMsg: db 'Error: Only Multiboot-compliant loaders are supported.',0
sizeErrMsg: db 'Error: Kernel size is too long!',0

[section .data]

EXPORT kCodeSel
  dd KERNEL_CODE_SEL

EXPORT kDataSel
  dd KERNEL_DATA_SEL

EXPORT uCodeSel
  dd USER_CODE_SEL

EXPORT uDataSel
  dd USER_DATA_SEL

EXPORT kTssSel
  dd KERNEL_TSS_SEL

EXPORT kResdTables
  dd KERNEL_RESD_TABLES

EXPORT idtLen
  dd KERNEL_IDT_LEN

EXPORT gdtLen
  dd KERNEL_GDT_LEN

EXPORT tssLen
  dd KERNEL_TSS_LEN

EXPORT kernelStackLen
  dd KERNEL_STACK_LEN

EXPORT idleStackLen
  dd IDLE_STACK_LEN
