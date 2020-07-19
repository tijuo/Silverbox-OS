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
; GRUB loads the kernel to its physical address. Memory needs
; to be accessed *extra* carefully here until paging is enabled.

[section .dtext progbits alloc exec nowrite align=16]

EXPORT start
;  eax has the mulitboot magic value
;  ebx has multiboot data address
  mov	esp, kBootStackTop
  mov	ebp, esp
  cld

  push eax
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
  mov  cx, KERNEL_DATA_SEL
  mov  ss, cx
  mov  fs, cx
  xor  cx, cx
  mov  gs, cx

;  sub esp, kVirtToPhys
  jmp BOOT_CODE_SEL:.reloadSel

.reloadSel:
 ; Load valid linear addresses into the GDTR

;  push kernelGDT
;  push KERNEL_GDT_LEN << 16
;  lgdt [esp+2]
;  add  esp, 8

  cmp eax, mbootMagic
  jne  .badMboot
  jmp .okMboot

.badMboot:
  jmp printMultibootErrMsg

.okMboot:
  call initPaging2

  call   init
  jmp stop

videoRamStart equ 0xA0000
videoBiosStart equ 0xC0000

initPaging2:
;kPageDir      equ 0x1000
;idleStackTop  equ 0x1000
;kLowPageTab   equ 0x2000
;kPageTab      equ 0x3000

  push ebp
  mov ebp, esp

  mov ax, es
  push eax
  mov ax, ds
  push eax

  ; Since we want to be writing to physical memory, use the
  ; data selector that 1:1 maps linear memory to physical memory

  mov ax, KERNEL_DATA_SEL
  mov es, ax
  mov ds, ax

  ; Clear the initial page directory, low memory page table, and kernel page table

  xor eax, eax
  mov ecx, 1024
  mov edi, kPageDir
  rep stosd

  mov ecx, 1024
  mov edi, kLowPageTab
  rep stosd

  mov ecx, 1024
  mov edi, kPageTab
  rep stosd

  mov ecx, 1024
  mov edi, k1to1PageTab
  rep stosd

  mov ebx, kLowPageTab
  xor ecx, ecx

.mapLowMemLoop:
  mov eax, ecx
  or  eax, PG_SUPERVISOR | PG_READ_WRITE | PG_PRESENT
  mov [ebx], eax

  cmp ecx, videoRamStart
  jge  .mapVideoRam

  add ecx, PAGE_SIZE
  add ebx, 4
  jmp .mapLowMemLoop

.mapVideoRam:
  mov ecx, videoRamStart
  mov ebx, kLowPageTab
  mov edx, videoRamStart
  shr edx, 12
  and edx, PAGE_SIZE-1
  lea ebx, [ebx+4*edx]

.mapVideoRamLoop:
  mov eax, ecx
  or  eax, PG_USER | PG_READ_WRITE | PG_CD | PG_PWT | PG_PRESENT
  mov [ebx], eax

  cmp ecx, videoBiosStart
  jge  .mapKernel

  add ecx, PAGE_SIZE
  add ebx, 4
  jmp .mapVideoRamLoop

.mapKernel:
  mov ebx, kPageTab
  mov esi, kPhysStart
  mov edi, kVirtStart
  shr edi, 12
  and edi, PAGE_SIZE - 1
  lea ebx, [ebx+edi*4]
  xor ecx, ecx

.mapKernelLoop:
  mov eax, esi
  or  eax, PG_SUPERVISOR | PG_READ_WRITE | PG_PRESENT
  mov [ebx], eax

  cmp ecx, kSize
  jge  .mapPDEs

  add ecx, PAGE_SIZE
  add esi, PAGE_SIZE
  add ebx, 4
  jmp .mapKernelLoop

.mapPDEs:
  mov ebx, kPageDir

  ; Map page directory onto itself

  mov ecx, 1023
  lea edx, [ebx+ecx*4]
  mov eax, kPageDir
  or  eax, PG_SUPERVISOR | PG_READ_WRITE | PG_PRESENT
  mov [edx], eax

  ; Map first page table (1:1 mapping of low memory)

  mov ecx, 0
  lea edx, [ebx+ecx*4]
  mov eax, kLowPageTab
  or  eax, PG_USER | PG_READ_WRITE | PG_PRESENT
  mov [edx], eax

  ; Map the page table for 1:1 kernel mapping (if necessary)

  mov  ecx, kPhysStart
  shr  ecx, 22
  lea  edx, [ebx+ecx*4]
  push ebx

  test dword [edx], PG_PRESENT
  jnz  .tablePresent
  xor  edi, edi
  mov  ebx, k1to1PageTab
  jmp  .map1to1Kernel

.tablePresent:
  mov edi, 1
  mov ebx, kLowPageTab

.map1to1Kernel:
  mov ecx, kPhysStart
  shr ecx, 12
  and ecx, PAGE_SIZE - 1
  lea ebx, [ebx+ecx*4]

  ; If a page table was needed for 1:1 kernel mapping, then
  ; map the kernel pages to the page table

  xor ecx, ecx
  mov esi, kPhysStart

.map1to1KernelLoop:
  mov eax, esi
  or  eax, PG_SUPERVISOR | PG_READ_WRITE | PG_PRESENT
  mov [ebx], eax

  cmp ecx, kSize
  jge  .restoreEBX

  add ecx, PAGE_SIZE
  add esi, PAGE_SIZE
  add ebx, 4
  jmp .map1to1KernelLoop

.restoreEBX:
  pop ebx
  test edi, edi
  jnz .setKPDE

.map1to1KernelPDE:
  mov  eax, k1to1PageTab
  or   eax, PG_SUPERVISOR | PG_READ_WRITE | PG_PRESENT
  mov  [edx], eax

  ; Map the kernel's page table

.setKPDE:
  mov ecx, kVirtStart
  shr ecx, 22
  lea edx, [ebx+ecx*4]
  mov eax, kPageTab
  or  eax, PG_SUPERVISOR | PG_READ_WRITE | PG_PRESENT
  mov [edx], eax

  mov   eax, kPageDir
  mov   cr3, eax

  mov   eax, cr4
  or    eax, CR4_PGE | CR4_PSE
  mov   cr4, eax

  mov   eax, cr0
  and   eax, ~(CR0_CD | CR0_NW) ; Enable normal caching
  or    eax, CR0_PG | CR0_WP    ; Enable paging, write-protection
  mov   cr0, eax

 ; Load valid linear addresses into the GDTR

  push kernelGDT
  push KERNEL_GDT_LEN << 16
  lgdt [esp+2]
  add  esp, 8

  mov   ax, KERNEL_DATA_SEL
  mov   ds, ax
  mov   es, ax
  mov   fs, ax
  mov   ss, ax

  jmp   KERNEL_CODE_SEL:.reloadCS

.reloadCS:
  leave
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
  mov bx, BOOT_DATA_SEL
  mov es, bx

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

EXPORT irqStackLen
  dd IRQ_STACK_LEN
