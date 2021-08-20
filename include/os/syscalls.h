#ifndef OS_SYSCALLS
#define OS_SYSCALLS

#include <types.h>
#include <oslib.h>
#include <os/msg/message.h>

#define SYSCALL_INT             0x40

#define SYS_EXIT		        0u
#define SYS_WAIT		        1u
#define SYS_SEND		        2u
#define SYS_RECEIVE		        3u
#define SYS_CALL		        4u
#define SYS_MAP			        5u
#define SYS_UNMAP		        6u
#define SYS_CREATE_THREAD	    7u
#define SYS_DESTROY_THREAD	    8u
#define SYS_READ_THREAD		    9u
#define SYS_UPDATE_THREAD	    10u
#define SYS_EOI			        11u
#define SYS_IRQ_WAIT		    12u
#define SYS_BIND_IRQ		    13u
#define SYS_UNBIND_IRQ		    14u

#define PM_PRESENT              0x01u
#define PM_NOT_PRESENT          0u
#define PM_READ_ONLY            0u
#define PM_READ_WRITE           0x02u
#define PM_NOT_CACHED           0x10u
#define PM_WRITE_THROUGH        0x08u
#define PM_NOT_ACCESSED         0u
#define PM_ACCESSED             0x20u
#define PM_NOT_DIRTY            0u
#define PM_DIRTY                0x40u
#define PM_LARGE_PAGE           0x80u
#define PM_OVERWRITE		0x20000000u
#define PM_ARRAY		0x40000000u
#define PM_INVALIDATE           0x80000000u

#define PRIV_SUPER			    0u
#define PRIV_PAGER			    1u

#define ESYS_OK			     	 0
#define ESYS_ARG		    	-1
#define ESYS_FAIL		    	-2
#define ESYS_PERM		    	-3
#define ESYS_BADCALL		    -4
#define ESYS_NOTIMPL		    -5
#define ESYS_NOTREADY           -6

#define ANY_SENDER		        NULL_TID

#define TF_STATUS		        1u
#define TF_PRIORITY		        2u
#define TF_REG_STATE		    	4u
#define TF_PMAP			        8u
#define TF_EXT_REG_STATE		16u

#define SYSCALL_TID_OFFSET      16u
#define SYSCALL_SUBJ_OFFSET     8u
#define SYSCALL_NUM_OFFSET	0u
#define SYSCALL_TID_MASK        0xFFFF0000u
#define SYSCALL_SUBJ_MASK       0xFF00u
#define SYSCALL_RET_MASK        0xFFu
#define SYSCALL_CALL_MASK       0xFFu
#define SYSCALL_NUM_MASK	SYSCALL_CALL_MASK

#define MSG_NOBLOCK         1
#define MSG_SYSTEM          2
#define MSG_CALL            4
#define MSG_KERNEL  0x80000000

typedef struct Tcb
{
  unsigned int priority;
  unsigned int status;
  paddr_t rootPageMap;
  tid_t waitTid;

  struct State
  {
    dword eax;
    dword ebx;
    dword ecx;
    dword edx;
    dword ebp;
    dword esp;
    dword edi;
    dword esi;
    dword eip;
    dword eflags;
    word  cs;
    word  ss;
  } state;
  void *extRegState;

} thread_info_t;

#ifdef __cplusplus
extern "C" {
#else
#endif /* __cplusplus */

void sys_exit(int code);
int sys_send(msg_t *msg);
int sys_call(msg_t *inMsg, msg_t *outMsg);
int sys_receive(msg_t *msg);
int sys_wait(int timeout);
int sys_map(paddr_t *rootPmap, void *vaddr, pframe_t *pframe, int numPages, unsigned int flags);
int sys_unmap(paddr_t *rootPmap, void *vaddr, int numPages, pframe_t *unmappedFrames);
tid_t sys_create_thread(tid_t tid, void *entry, paddr_t *rootPmap, void *stackTop);
int sys_destroy_thread(tid_t tid);
int sys_read_thread(tid_t tid, unsigned int flags, thread_info_t *info);
int sys_update_thread(tid_t tid, unsigned int flags, thread_info_t *info);
int sys_bind_irq(tid_t tid, unsigned int irqNum);
int sys_unbind_irq(unsigned int irqNum);
int sys_eoi(unsigned int irqNum);
int sys_wait_irq(unsigned int irqNum);
int sys_poll_irq(unsigned int irqNum);

#ifdef __cplusplus
};
#else
#endif /* __cplusplus */
#endif /* OS_SYSCALLS */
