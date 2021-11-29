#ifndef KERNEL_SYSCALL_H
#define KERNEL_SYSCALL_H

#include <os/syscalls.h>
#include <util.h>
#include <stdnoreturn.h>

extern noreturn NAKED void sysenter_entry(void);

#endif /* KERNEL_SYSCALL_H */
