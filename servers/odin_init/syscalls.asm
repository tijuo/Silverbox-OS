[global syscall0]
[global syscall1]
[global syscall2]
[global syscall3]
[global syscall4]

[BITS 32]
[section .text]

syscall0:
    push ebp                ; save the old frame pointer
    mov ebp, esp

    mov eax, [ebp + 8]      ; syscall number
    push ebp                ; save the current frame pointer
    lea edx, .ret_addr
    mov ebp, esp
    sysenter

.ret_addr:
    mov esp, ebp
    pop ebp

    mov ecx, [ebp + 12]
    mov dword [ecx], ebx
    mov dword [ecx + 4], edx
    mov dword [ecx + 8], esi
    mov dword [ecx + 12], edi
    leave
    ret

syscall1:
    push ebp                ; save the old frame pointer
    mov ebp, esp

    mov eax, [ebp + 8]      ; syscall number
    mov ebx, [ebp + 12]     ; arg 1
    push ebp                ; save the current frame pointer
    lea edx, .ret_addr
    mov ebp, esp
    sysenter

.ret_addr:
    mov esp, ebp
    pop ebp

    mov ecx, [ebp + 16]
    mov dword [ecx], ebx
    mov dword [ecx + 4], edx
    mov dword [ecx + 8], esi
    mov dword [ecx + 12], edi
    leave
    ret

syscall2:
    push ebp                ; save the old frame pointer
    mov ebp, esp

    mov eax, [ebp + 8]      ; syscall number
    mov ebx, [ebp + 12]     ; arg 1
    mov ecx, [ebp + 16]     ; arg 2
    push ebp                ; save the current frame pointer
    lea edx, .ret_addr
    mov ebp, esp
    sysenter

.ret_addr:
    mov esp, ebp
    pop ebp

    mov ecx, [ebp + 20]
    mov dword [ecx], ebx
    mov dword [ecx + 4], edx
    mov dword [ecx + 8], esi
    mov dword [ecx + 12], edi
    leave
    ret

syscall3:
    push ebp                ; save the old frame pointer
    mov ebp, esp

    mov eax, [ebp + 8]      ; syscall number
    mov ebx, [ebp + 12]     ; arg 1
    mov ecx, [ebp + 16]     ; arg 2
    mov esi, [ebp + 20]     ; arg 3
    push ebp                ; save the current frame pointer
    lea edx, .ret_addr
    mov ebp, esp
    sysenter

.ret_addr:
    mov esp, ebp
    pop ebp

    mov ecx, [ebp + 24]
    mov dword [ecx], ebx
    mov dword [ecx + 4], edx
    mov dword [ecx + 8], esi
    mov dword [ecx + 12], edi
    leave
    ret

syscall4:
    push ebp                ; save the old frame pointer
    mov ebp, esp

    mov eax, [ebp + 8]      ; syscall number
    mov ebx, [ebp + 12]     ; arg1
    mov ecx, [ebp + 16]     ; arg2
    mov esi, [ebp + 20]     ; arg3
    mov edi, [ebp + 24]     ; arg4
    push ebp                ; save the current frame pointer
    lea edx, .ret_addr
    mov ebp, esp
    sysenter

.ret_addr:
    mov esp, ebp
    pop ebp

    mov ecx, [ebp + 28]
    mov dword [ecx], ebx
    mov dword [ecx + 4], edx
    mov dword [ecx + 8], esi
    mov dword [ecx + 12], edi
    leave
    ret
