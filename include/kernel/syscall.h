#if 0
#ifndef SYSCALL_H
#define SYSCALL_H

#include <kernel/thread.h>
#include <oslib.h>

int sysMap( TCB *tcb, addr_t virt, addr_t phys, size_t pages, int flags, addr_t addrSpace );
int sysMapPageTable( TCB *tcb, addr_t virt, addr_t phys, int flags, addr_t addrSpace );
int sysGrant( TCB *tcb, addr_t srcAddr, addr_t destAddr,
    addr_t addrSpace, size_t pages );
int sysGrantPageTable( TCB *tcb, addr_t srcAddr, addr_t destAddr,
    addr_t addrSpace, size_t tables );
addr_t sysUnmap( TCB *tcb, addr_t virt, addr_t addrSpace );
addr_t sysUnmapPageTable( TCB *tcb, addr_t virt, addr_t addrSpace );


#endif /* SYSCALL_H */
#endif /* 0 */
