#include <util.h>
#include <stdnoreturn.h>
#include <stddef.h>

noreturn void syscall_entry(void) NAKED;

void *syscall_table[] = {
NULL
};

/* todo: This function needs to save the user stack, and then load
         the kernel stack without touching any of the registers. */

void syscall_entry(void) {
__asm__(
	"cmp $12, %rax\n"
	"ja 2f\n"
        "xchg tss+4, %rsp\n"
        "push tss+4\n"
        "mov %rsp, tss+4\n"
        "add $8, tss+4\n"
	"xchg %rbx, %rcx\n"
	"push %rbx\n"
	"push %r11\n"
	"sub $4, %rsp\n"
	"lea syscall_table, %rbx\n"
	"lea (%rbx,%rax,8), %rax\n"
	"call *%rax\n"
	"add $4, %rsp\n"
	"pop %r11\n"
	"pop %rcx\n"
	"jmp 1f\n"
	"2:\n"
	"movq $-4, %rax\n"
	"1:\n"
	"sysretq\n");
}
