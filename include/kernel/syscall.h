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
void *sysUnmap( TCB *tcb, void *virt, addr_t addrSpace );
void *sysUnmapPageTable( TCB *tcb, void *virt, addr_t addrSpace );

/*
int sysGrant( PCB *tcb, addr_t srcAddr, addr_t destAddr, pid_t destID, size_t pages );
int sysMap( PCB *tcb, addr_t virt, addr_t phys, size_t pages );
int sysRegisterInt( PCB *process, mid_t mailbox, int intNum );
int sysEndPageFault( tid_t tid, PCB *process );
int sysEndIRQ( int irq );
int handleKernelMsg( TCB *process, struct MessageHeader *header );
*/
#endif /* SYSCALL_H */
