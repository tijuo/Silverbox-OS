#ifndef KERNEL_SYSCALL_H
#define KERNEL_SYSCALL_H

#include <os/syscalls.h>

extern noreturn void sysenter(void) __attribute__((naked));

#endif /* KERNEL_SYSCALL_H */
