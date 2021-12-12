#include <util.h>
#include <stdnoreturn.h>

noreturn void syscall_entry(void) NAKED;

void *syscall_table[] = {
};

void syscall_entry(void) {
__asm__(
	"cmp $12, %rax\n"
	"ja 2f\n"
	"mov %rsp, tss+4\n"
	"lea kernel_stack_top, %rsp\n"
	"xchg %rbx, %rcx\n"
	"push %rbx\n"
	"push %r11\n"
	"sub $8, %rsp\n"
	"lea syscall_table, %rbx\n"
	"lea (%rbx,%rax,8), %rax\n"
	"call *%rax\n"
	"add $8, %rsp\n"
	"pop %r11\n"
	"pop %rcx\n"
	"jmp 1f\n"
	"2:\n"
	"movq $-4, %rax\n"
	"1:\n"
	"sysretq\n");
}
