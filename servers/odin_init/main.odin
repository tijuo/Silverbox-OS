package init_server

import "base:runtime"

SyscallOut :: [4]int

foreign import syscall "syscalls.asm"

foreign syscall {
    syscall0 :: proc(number: int, out: ^SyscallOut) -> int ---
    syscall1 :: proc(number: int, arg1: int, out: ^SyscallOut) -> int ---
    syscall2 :: proc(number: int, arg1: int, arg2: int, out: ^SyscallOut) -> int ---
    syscall3 :: proc(number: int, arg1: int, arg2: int, arg3: int, out: ^SyscallOut) -> int ---
    syscall4 :: proc(number: int, arg1: int, arg2: int, arg3: int, arg4: int, out: ^SyscallOut) -> int ---
}

syscall :: proc {
    syscall0,
    syscall1,
    syscall2,
    syscall3,
    syscall4,
}

_start :: proc "cdecl" (multiboot_info: rawptr, first_free_page: uint, stack_size: uint) -> ! {
    context = runtime.default_context()
    main(multiboot_info, first_free_page, stack_size)

    for {}
}

main :: proc(multiboot_info: rawptr, first_free_page: uint, stack_size: uint) {
    eprintfln("Hello, world!")
}