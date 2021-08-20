#ifndef KERNEL_SYSCALL_H
#define KERNEL_SYSCALL_H

#include <os/syscalls.h>

#define SYSCALL_NUM(state)      (unsigned int)(((state)->eax & SYSCALL_NUM_MASK) >> SYSCALL_NUM_OFFSET)
#define SYSCALL_TID(state)      (tid_t)(((state)->eax & SYSCALL_TID_MASK) >> SYSCALL_TID_OFFSET)
#define SYSCALL_SUBJ(state)     (unsigned char)(((state)->eax & SYSCALL_SUBJ_MASK) >> SYSCALL_SUBJ_OFFSET)

#endif /* KERNEL_SYSCALL_H */
