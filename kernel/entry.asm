[BITS 32]

[extern init]
[global boot_stack_top]
[global start]
[global kernel_gdt]

MB2_HEADER_MAGIC 	equ 0xE85250D6
MB2_ARCH 		equ 0
MB2_HEADER_LEN 		equ (mboot_end - mboot)
MB2_CHECKSUM 		equ 0xFFFFFFFF & (-(MB2_HEADER_MAGIC + MB2_ARCH + MB2_HEADER_LEN))

MB2_LOADER_MAGIC 	equ 0x36d76289

MB2_FLAG_OPTIONAL	equ 1
MB2_MODULE_ALIGN_TYPE 	equ 6
MB2_INFO_TYPE		equ 1
MB2_INFO_MEM		equ 4
MB2_INFO_BOOT   	equ 5
MB2_INFO_CMDLINE	equ 1
MB2_INFO_MODULES	equ 3
MB2_INFO_MMAP		equ 6
MB2_INFO_FBUF		equ 8
MB2_INFO_EFI64		equ 12
MB2_INFO_SMBIOS		equ 13
MB2_INFO_RSDP		equ 15 ; ACPI 2.0+
MB2_INFO_EFI_MMAP	equ 17
MB2_INFO_BOOT_SVC	equ 18 ; EFI Boot services not terminated
MB2_INFO_EFI_HANDLE	equ 20 ; EFI64 image handle
MB2_END_TYPE		equ 0

CPUID_EDX_MSR			equ (1 << 5)
CPUID_EDX_PAE			equ (1 << 6)
CPUID_EDX_APIC			equ (1 << 9)
CPUID_EDX_SEP			equ (1 << 11)
CPUID_EDX_PGE			equ (1 << 13)
CPUID_EDX_FXSR			equ (1 << 24)
CPUID_EDX_SSE			equ (1 << 25)
CPUID_EDX_SSE2			equ (1 << 26)

CPUID_EDX			equ CPUID_EDX_MSR | CPUID_EDX_PAE | CPUID_EDX_APIC | CPUID_EDX_SEP | CPUID_EDX_PGE | CPUID_EDX_FXSR | CPUID_EDX_SSE | CPUID_EDX_SSE2

CPUID_ECX_SSE3			equ (1 << 0)

CPUID_SYSCALL			equ (1 << 11)
CPUID_NX			equ (1 << 20)
CPUID_1GB			equ (1 << 26)
CPUID_LONG_MODE			equ (1 << 29)

CPUID_MAX_PADDR_MASK	equ 0xFF

CR4_DE					equ (1 << 3)
CR4_PAE					equ (1 << 5)
CR4_PGE					equ (1 << 7)
CR4_OSFXSR				equ (1 << 9)
CR4_OSXMMEXCPT			equ (1 << 10)

CR0_PG					equ (1 << 31)
CR0_CD					equ (1 << 30)
CR0_NW					equ (1 << 29)

EFER_MSR				equ 0xC0000080

EFER_SCE				equ (1 << 0)
EFER_LME				equ (1 << 8)
EFER_NX					equ (1 << 11)

[section .header]

mboot:
  dd MB2_HEADER_MAGIC
  dd MB2_ARCH
  dd MB2_HEADER_LEN
  dd MB2_CHECKSUM

.mod_align_start:
  dw MB2_MODULE_ALIGN_TYPE
  dw 0
  dd (.mod_align_end-.mod_align_start)
.mod_align_end:

.info_start:
  dw MB2_INFO_TYPE
  dw MB2_FLAG_OPTIONAL
  dd (.info_end-.info_start)
  dd MB2_INFO_MEM
  dd MB2_INFO_BOOT
  dd MB2_INFO_CMDLINE
  dd MB2_INFO_MODULES
  dd MB2_INFO_MMAP
  dd MB2_INFO_FBUF
  dd MB2_INFO_EFI64
  dd MB2_INFO_SMBIOS
  dd MB2_INFO_RSDP
  dd MB2_INFO_EFI_MMAP
  dd MB2_INFO_BOOT_SVC
  dd MB2_INFO_EFI_HANDLE
.info_end:

 dw MB2_END_TYPE
 dw 0
 dd 8
mboot_end:

[section .btext]
[extern max_phys_addr]
[extern is_1gb_pages_supported]

start:
  cli
  lea esp, [boot_stack_top]
  mov ebp, esp

; Save multiboot2 header

  cmp eax, MB2_LOADER_MAGIC
  jne stop_init.bad_multiboot2

  push ebx

; Check for needed CPUID features

; Determine whether this processor is a pentium 4 or newer

  mov eax, 0
  cpuid

  cmp eax, 5
  jl $stop_init.old_processor

  mov eax, 0x80000000
  cpuid

  cmp eax, 0x80000008
  jl $stop_init.old_processor

; Check for needed cpuid features

  mov eax, 1
  cpuid

  not edx
  test edx, CPUID_EDX
  jnz $stop_init.old_processor

  not ecx
  test ecx, CPUID_ECX_SSE3
  jnz $stop_init.old_processor

  mov eax, 0x80000001
  cpuid

  test edx, CPUID_LONG_MODE
  jz $stop_init.long_mode_missing

; Load gdt and set data segment selectors to known values

  push dword $kernel_gdt
  push dword (0x30 << 16)
  lgdt [esp+2]

  add esp, 8

.load_segment_sel:

  mov dx, 0x10
  mov ds, dx
  mov es, dx
  mov ss, dx
  xor dx, dx
  mov fs, dx
  mov gs, dx

  jmp 0x28:.reload_cs1

.reload_cs1:

  lea ebx, [pml4_table]
  mov cr3, ebx

  lea edx, [identity_pdpt]
  lea esi, [identity_pdir]

; Set first PDPT for the identity mapping

  mov ecx, edx
  or  ecx, 0x03
  mov [ebx], ecx

; Set first page directory for the identity mapping

  mov ecx, esi
  or  ecx, 0x03
  mov [edx], ecx

  mov ecx, 512
  xor eax, eax
  xor edi, edi
.map_large_pages:
  or dword [esi+eax*8], edi
  inc eax
  add edi, 0x200000
  loop .map_large_pages

; Enable debug extensions, PAE and global pages, FXSAVE/FXRSTOR,
; and unmasked SIMD floating point exceptions

  mov eax, cr4
  or eax, (CR4_DE | CR4_PAE | CR4_PGE | CR4_OSFXSR | CR4_OSXMMEXCPT)
  mov cr4, eax

; Enable long mode, syscall, and execute disable
  mov ecx, EFER_MSR
  rdmsr
  or  eax, (EFER_SCE | EFER_LME | EFER_NX)
  wrmsr

; Enable paging and fully enter long mode

  mov eax, cr0
  or eax, CR0_PG  ; Enable paging
  and eax, ~(CR0_CD | CR0_NW) ; Enable cache
  mov cr0, eax

  pop edi

; Enter long mode

  jmp 0x08:reload_cs2

stop_init:
.bad_multiboot2:
  push dword 0
  push dword 0
  push dword $init_error_msg
  jmp .print

.low_bits:
  push dword 0
  push dword 0
  push dword $low_bits_msg
  jmp .print

.long_mode_missing:
  push dword 0
  push dword 0
  push dword $long_mode_missing_msg
  jmp .print

.syscall_missing:
  push dword 0
  push dword 0
  push dword $syscall_missing_msg
  jmp .print

.nx_bit_missing:
  push dword 0
  push dword 0
  push dword $nx_bit_missing_msg
  jmp .print

.old_processor:
  push dword 0
  push dword 0
  push dword $old_processor_msg

.print:
  call print_message

.stop:
  cli
  hlt
  jmp .stop

; Prints a message on the screen (assumes 80 cols, 25 rows)

; [ebp+8] - C string to print (null-terminated)
; [ebp+12] - x offset
; [ebp+16] - y offset

print_message:
  push ebp
  mov ebp, esp

  pushf
  push edi
  push esi

  cld
  mov edi, [ebp+8]
  mov esi, edi
  xor eax, eax

.count_str_bytes:
  scasb
  je .print_msg
  jmp .count_str_bytes

.print_msg:
  sub edi, esi
  mov ecx, edi

  mov eax, [ebp+16]
  xor edx, edx
  mov edi, 25
  mul edi
  add eax, [ebp+12]
  and eax, 0x8000
  add eax, 0xB8000
  mov edi, eax

  mov eax, 0x07

.print_char:
  movsb
  stosb
  loop .print_char

  pop esi
  pop edi
  popf

  leave
  ret

init_error_msg: db "Error: Invalid multiboot2 magic.", 0
old_processor_msg: db "Error: Processor must be a Pentium 4 Prescott or newer.", 0
low_bits_msg: db "Error: Processor should support at least 1 TiB of physical memory.", 0
long_mode_missing_msg: db "Error: Processor doesn't support 64-bit mode.", 0
syscall_missing_msg: db "Error: Processor doesn't support SYSCALL/SYSRET instrutions.", 0
nx_bit_missing_msg: db "Error: Processor doesn't support NX bit.", 0

[BITS 64]

reload_cs2:
  ; Test for syscall/sysret and NX bit

  mov eax, 0x80000001
  cpuid

  ; Determine if 1 GB pages are supported

  test edx, CPUID_1GB
  setnz byte [is_1gb_pages_supported]

  test edx, CPUID_NX
  jnz .test_syscall
  push dword 0x28
  push dword stop_init.syscall_missing
  retf

.test_syscall:
  test edx, CPUID_SYSCALL
  jnz .get_max_bits
  push dword 0x28
  push dword stop_init.syscall_missing
  retf

  ; Get the maximum physical address bits from CPUID

.get_max_bits:
  mov eax, 0x80000008
  cpuid

  cmp al, 40
  jb $stop_init.low_bits

  mov rdx, 1
  mov cl, al
  shl rdx, cl
  dec rdx

  lea rbx, [max_phys_addr]
  mov [rbx], rdx

  add rsp, 16
  mov rax, $init
  jmp rax

[section .bdata]

align 16
boot_stack:
  times 131072 db 0
boot_stack_top:

[section .data]

align 4096
pml4_table:
  times 512 dq 0

align 4096
identity_pdpt:
  times 512 dq 0

align 4096
identity_pdir:
; Bits are already set to global, r/w, present, 2 MiB page-sized
; Base address just needs to be filled in

  times 512 dq 0x183

align 16
kernel_gdt:
  dq 0
  dq 0x00AF98000000FFFF ; 64-bit kernel code
  dq 0x00CF92000000FFFF ; Kernel data
  dq 0x00AFF8000000FFFF ; 64-bit user code
  dq 0x00CFF2000000FFFF ; User data
  dq 0x00CF98000000FFFF ; 32-bit boot code
  dq 0
