#ifndef OS_SYSCALLS
#define OS_SYSCALLS

#include <types.h>
#include <oslib.h>
#include <os/msg/message.h>

#define SYSCALL_INT             0x40

#define SYS_EXIT		0
#define SYS_WAIT		1
#define SYS_SEND		2
#define SYS_RECEIVE		3
#define SYS_SEND_WAIT		4
#define SYS_RECEIVE_WAIT	5
#define SYS_CALL		6
#define SYS_CALL_WAIT		7
#define SYS_MAP			8
#define SYS_UNMAP		9
#define SYS_CREATE_THREAD	10
#define SYS_DESTROY_THREAD	11
#define SYS_READ_THREAD		12
#define SYS_UPDATE_THREAD	13
#define SYS_EOI			14
#define SYS_IRQ_WAIT		15
#define SYS_BIND_IRQ		16
#define SYS_UNBIND_IRQ		17

#define PM_PRESENT              0x01
#define PM_NOT_PRESENT          0
#define PM_READ_ONLY            0
#define PM_READ_WRITE           0x02
#define PM_NOT_CACHED           0x10
#define PM_WRITE_THROUGH        0x08
#define PM_NOT_ACCESSED         0
#define PM_ACCESSED             0x20
#define PM_NOT_DIRTY            0
#define PM_DIRTY                0x40
#define PM_LARGE_PAGE           0x80
#define PM_INVALIDATE           0x80000000

#define PRIV_SUPER			0
#define PRIV_PAGER			1

#define ESYS_OK			     	 0
#define ESYS_ARG		    	-1
#define ESYS_FAIL		    	-2
#define ESYS_PERM		    	-3
#define ESYS_BADCALL		    	-4
#define ESYS_NOTIMPL		    	-5
#define ESYS_NOTREADY               	-6

#define ANY_SENDER		NULL_TID

#define TF_STATUS		1
#define TF_PRIORITY		2
#define TF_REG_STATE		4
#define TF_PMAP			8

typedef struct Tcb
{
  int priority;
  int status;
  paddr_t rootPageMap;
  tid_t waitTid;

  struct State
  {
    dword eax, ebx, ecx, edx, ebp, esp, edi, esi;
    dword eip, eflags;
  } state;
} thread_info_t;

#ifdef __cplusplus
extern "C" {
#else
#endif /* __cplusplus */

void sys_exit(int code);
int sys_send(const msg_t *msg, int block);
int sys_call(const msg_t *inMsg, msg_t *outMsg, int block);
int sys_receive(msg_t *msg, int block);
int sys_wait(unsigned int timeout);
int sys_map(u32 rootPmap, addr_t vaddr, pframe_t pframe, size_t numPages, int flags);
int sys_unmap(u32 rootPmap, addr_t vaddr, size_t numPages);
tid_t sys_create_thread(tid_t tid, addr_t entry, u32 rootPmap, addr_t stackTop);
int sys_destroy_thread(tid_t tid);
int sys_read_thread(tid_t tid, int flags, thread_info_t *info);
int sys_update_thread(tid_t tid, int flags, thread_info_t *info);
int sys_bind_irq(tid_t tid, int irqNum);
int sys_unbind_irq(int irqNum);
int sys_eoi(int irqNum);
int sys_wait_irq(int irqNum);
int sys_poll_irq(int irqNum);

#ifdef __cplusplus
};
#else
#endif /* __cplusplus */
#endif /* OS_SYSCALLS */
