#ifndef OS_SYSCALLS
#define OS_SYSCALLS

#include <types.h>
#include <oslib.h>
#include <os/msg/message.h>

#define SYS_GRANT				0u
#define SYS_REVOKE				1u
#define SYS_SEND		        		2u
#define SYS_RECV		        	  3u
#define SYS_SEND_AND_RECV       4u

#define SYS_CREATE_THREAD    		7u
#define SYS_DESTROY_THREAD	    8u
#define SYS_READ_THREAD		    	9u
#define SYS_UPDATE_THREAD	    	10u
#define SYS_POLL								11u
#define SYS_WAIT									12u

#define PM_UNMAPPED             0x01u
#define PM_READ_ONLY            0x02u
#define PM_KERNEL               0x04u
#define PM_UNCACHED             0x08u
#define PM_WRITETHRU            0x10u       // Write through
#define PM_WRITECOMB            0x20u       // Write combining
#define PM_DIRTY                0x40u
#define PM_ACCESSED             0x80u
#define PM_PAGE_SIZED           0x100u      // PDE points to a large page instead of table
#define PM_STICKY               0x200u      // Suggestion to not invalidate entry upon context switch
#define PM_CLEAR				0x400u		// Clear the contents of a frame before mapping
#define PM_AVAIL_MASK           0x1F000u    // Mask for flags that encode available bits
#define PM_AVAIL_OFFSET         12
#define PM_OVERWRITE            0x8000u
#define PM_ARRAY                0x4000u

/*
 #define PM_PRESENT              0x01u
 #define PM_NOT_PRESENT          0u
 #define PM_READ_ONLY            0u
 #define PM_READ_WRITE           0x02u
 #define PM_WRITE_THROUGH        0x08u
 #define PM_NOT_CACHED           0x10u
 #define PM_NOT_ACCESSED         0u
 #define PM_ACCESSED             0x20u
 #define PM_NOT_DIRTY            0u
 #define PM_DIRTY                0x40u
 #define PM_LARGE_PAGE           0x80u
 #define PM_GLOBAL               0x100
 #define PM_CACHE_WB             0x00u
 #define PM_CACHE_UC             0x18u
 #define PM_CACHE_UC_MINUS       0x10u
 #define PM_CACHE_WT             0x08u


 //#define PM_CACHE_WC
 //#define PM_CACHE_WP

 #define PUNMAP_LARGE_PAGE       0x01u

 #define PM_OVERWRITE		    0x20000000u
 #define PM_ARRAY		        0x40000000u
 #define PM_INVALIDATE           0x80000000u
 */

#define PRIV_SUPER		0u
#define PRIV_PAGER		1u

#define ESYS_OK			0l
#define ESYS_ARG		-1l
#define ESYS_FAIL		-2l
#define ESYS_PERM		-3l
#define ESYS_BADCALL		-4l
#define ESYS_NOTIMPL		-5l
#define ESYS_NOTREADY         	-6l
#define ESYS_INT              	-7l

#define TF_STATUS		1l
#define TF_PRIORITY		2l
#define TF_REG_STATE		4l
#define TF_ROOT_PMAP		8l
#define TF_EXT_REG_STATE	16l
#define TF_CAP_TABLE		32l
#define TF_EX_HANDLER		64l
#define TF_EVENTS		128l

#define EV_STOP_CHILD	(1l << 0)
#define EV_STOP_PARENT 	(1l << 1)
#define EV_STOP_EH 	(1l << 2)
#define EV_STOP_PAGER 	(1l << 3)
#define EV_IRQ0 	(1l << 8)
#define EV_IRQ1 	(1l << 9)
#define EV_IRQ2 	(1l << 10)
#define EV_IRQ3 	(1l << 11)
#define EV_IRQ4 	(1l << 12)
#define EV_IRQ5 	(1l << 13)
#define EV_IRQ6 	(1l << 14)
#define EV_IRQ7 	(1l << 15)
#define EV_IRQ8 	(1l << 16)
#define EV_IRQ9 	(1l << 17)
#define EV_IRQ10 	(1l << 18)
#define EV_IRQ11 	(1l << 19)
#define EV_IRQ12 	(1l << 20)
#define EV_IRQ13 	(1l << 21)
#define EV_IRQ14 	(1l << 22)
#define EV_IRQ15 	(1l << 23)
#define EV_IRQ16 	(1l << 24)
#define EV_IRQ17 	(1l << 25)
#define EV_IRQ18 	(1l << 26)
#define EV_IRQ19 	(1l << 27)
#define EV_IRQ20 	(1l << 28)
#define EV_IRQ21 	(1l << 29)
#define EV_IRQ22 	(1l << 30)
#define EV_IRQ23 	(1l << 31)
#define EV_RESD		(1l << 63)

struct PageMapping {
  union {
	pframe_t phys_frame;
	long int block_num;
  };
  long int flags;
};

typedef struct Tcb {
  struct State {
	uint64_t rax;
	uint64_t rbx;
	uint64_t rcx;
	uint64_t rdx;
	uint64_t rdi;
	uint64_t rsi;
	uint64_t rbp;
	uint64_t rsp;
	uint64_t r8;
	uint64_t r9;
	uint64_t r10;
	uint64_t r11;
	uint64_t r12;
	uint64_t r13;
	uint64_t r14;
	uint64_t r15;
	uint64_t rip;
	uint64_t rflags;

	uint16_t cs;
	uint16_t ds;
	uint16_t es;
	uint16_t fs;

	uint16_t gs;
	uint16_t ss;
  } state;

  tid_t tid;

  uint64_t root_pmap;
  uint8_t status;
  uint8_t current_processor_id;

  uint8_t priority;

  uint64_t pending_events;
  uint64_t event_mask;

  void *capability_table;
  size_t capability_table_len;

  tid_t exception_handler;
  tid_t pager;

  tid_t wait_tid;
  tid_t parent;

  tid_t nextSibling;
  tid_t prevSibling;

  tid_t childrenHead;
  tid_t childrenTail;

  tid_t receiverWaitHead;
  tid_t receiverWaitTail;

  tid_t senderWaitHead;
  tid_t senderWaitTail;

  xsave_state_t *xsave_state;

} thread_info_t;

#ifdef __cplusplus
extern "C" {
#else
#endif /* __cplusplus */

#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNU__MINOR__ >= 9)

#define SYSCALL1(syscallName, type1, arg1) \
static inline long int sys_##syscallName(type1 arg1) { \
	long int ret_val; \
	long int dummy; \
	asm volatile("pushq %%r11\n" \
				"pushq %%rcx\n" \
				"syscall\n" \
				"popq %%rcx\n" \
				"popq %%r11\n" \
					: "=a"(ret_val), "=D"(dummy) \
					: "a"(SYS_##syscallName), \
					  "D"(arg1) \
					: "memory"); \
	return ret_val; \
}

// Dummy variable has to be used because input-only arguments can't be modified

#define SYSCALL2(syscallName, type1, arg1, type2, arg2) \
static inline long int sys_##syscallName(type1 arg1, type2 arg2) { \
	long int ret_val; \
	long int dummy; \
	long int dummy2; \
	asm volatile("pushq %%r11\n" \
				"pushq %%rcx\n" \
				"syscall\n" \
				"popq %%rcx\n" \
				"popq %%r11\n" \
					: "=a"(ret_val), "=D"(dummy), "=S"(dummy2) \
					: "a"(SYS_##syscallName), \
					  "D"(arg1), "S"(arg2) \
					: "memory"); \
	return ret_val; \
}

#define SYSCALL3(syscallName, type1, arg1, type2, arg2, type3, arg3) \
static inline long int sys_##syscallName(type1 arg1, type2 arg2, type3 arg3) { \
	long int ret_val; \
	long int dummy; \
	long int dummy2; \
	long int dummy3; \
	asm volatile("pushq %%r11\n" \
				"pushq %%rcx\n" \
				"syscall\n" \
				"popq %%rcx\n" \
				"popq %%r11\n" \
					: "=a"(ret_val), "=D"(dummy), "=S"(dummy2), "=d"(dummy3) \
					: "a"(SYS_##syscallName), \
					  "D"(arg1), "S"(arg2), "d"(arg3) \
					: "memory"); \
	return ret_val; \
}

#define SYSCALL4(syscallName, type1, arg1, type2, arg2, type3, arg3, type4, arg4) \
static inline long int sys_##syscallName(type1 arg1, type2 arg2, type3 arg3, type4 arg4) { \
	long int ret_val; \
	long int dummy; \
	long int dummy2; \
	long int dummy3; \
	long int dummy4; \
	asm volatile("pushq %%r11\n" \
				"pushq %%rcx\n" \
				"syscall\n" \
				"popq %%rcx\n" \
				"popq %%r11\n" \
					: "=a"(ret_val), "=D"(dummy), "=S"(dummy2), "=d"(dummy3), "=b"(dummy4) \
					: "a"(SYS_##syscallName), \
					  "D"(arg1), "S"(arg2), "d"(arg3), "b"(arg4) \
					: "memory"); \
	return ret_val; \
}

#else
#error "GCC 4.9 or greater is required."
#endif /* __GNUC__ > 4 || (__GNUC__ == 4 && __GNU__MINOR__ >= 9) */

#define SYS_grant	SYS_GRANT
#define SYS_revoke 	SYS_REVOKE
#define SYS_send 								SYS_SEND
#define SYS_recv 								SYS_RECV
#define SYS_send_and_recv 			SYS_SEND_AND_RECV
#define SYS_create_thread 			SYS_CREATE_THREAD
#define SYS_destroy_thread 			SYS_DESTROY_THREAD
#define SYS_read_thread 				SYS_READ_THREAD
#define SYS_update_thread 			SYS_UPDATE_THREAD
#define SYS_poll					SYS_POLL
#define SYS_wait					SYS_WAIT

#define SYS_get_page_mappings_base	SYS_get_page_mappings
#define SYS_set_page_mappings_base SYS_set_page_mappings
#define SYS_create_thread_base 		SYS_create_thread
#define SYS_recv_base 						SYS_recv
#define SYS_send_and_recv_base 		SYS_send_and_recv

SYSCALL3(send, tid_t, recipient, long int, subject, long int, flags)
SYSCALL4(send_and_recv_base, tid_t, recipient, tid_t, replier, long int,
		 sendFlags, long int, recvFlags)
SYSCALL3(create_thread_base, void *, entry, paddr_t, rootPmap, void *,
		 stackTop)
SYSCALL2(read_thread, tid_t, tid, thread_info_t *, info)
SYSCALL3(update_thread, tid_t, tid, long int, flags, thread_info_t *, info)
SYSCALL1(destroy_thread, tid_t, tid)
SYSCALL1(poll, long int, mask)
SYSCALL1(wait, long int, mask)

static inline tid_t sys_create_thread(void *entry, paddr_t rootPmap,
									  void *stackTop)
{
  return (tid_t)sys_create_thread_base(entry, rootPmap, stackTop);
}

static inline long int sys_send_and_recv(tid_t recipient, tid_t replier,
										 long int subject,
										 long int sendFlags,
										 long int recvFlags)
{
  long int ret_val;
  long int dummy;
  asm volatile("pushq %%rbp\n"
	  "pushq %%rdx\n"
	  "pushq %%rcx\n"
	  "movq 1f, %%rcx\n"
	  "movq %%rsp, %%rbp\n"
	  "sysenter\n"
	  "1:\n"
	  "movq %%rbp, %%rsp\n"
	  "popq %%rcx\n"
	  "popq %%rdx\n"
	  "popq %%rbp\n"
	  : "=a"(ret_val), "=d"(dummy)
	  : "a"(SYS_SEND_AND_RECV),
	  "b"((long int)replier << 16 | (long int)recipient), "d"(subject),
	  "S"(sendFlags), "D"(recvFlags)
	  : "cc", "memory");
  return ret_val;
}

static inline long int sys_reply_and_wait(tid_t recipient, long int subject,
										  long int replyFlags,
										  long int waitFlags)
{
  return sys_send_and_recv(recipient, NULL_TID, subject, replyFlags, waitFlags);
}

static inline long int sys_recv(tid_t sender, long int flags,
								tid_t *actualSender)
{
  long int ret_val;
  long int dummy;
  long int actual;

  asm volatile("pushq %%rbp\n"
	  "pushq %%rdx\n"
	  "pushq %%rcx\n"
	  "movq 1f, %%rcx\n"
	  "movq %%rsp, %%rbp\n"
	  "sysenter\n"
	  "1:\n"
	  "movq %%rbp, %%rsp\n"
	  "popq %%rcx\n"
	  "popq %%rdx\n"
	  "popq %%rbp\n"
	  : "=a"(ret_val), "=b"(actual), "=d"(dummy)
	  : "a"(SYS_RECV),
	  "b"(sender), "d"(flags)
	  : "cc", "memory");

  if(actualSender)
	*actualSender = (tid_t)actual;

  return ret_val;
}

#undef SYS_grant
#undef SYS_revoke
#undef SYS_send
#undef SYS_recv
#undef SYS_send_and_recv
#undef SYS_create_thread
#undef SYS_destroy_thread
#undef SYS_read_thread
#undef SYS_update_thread
#undef SYS_get_page_mappings_base
#undef SYS_set_page_mappings_base
#undef SYS_create_thread_base
#undef SYS_recv_base
#undef SYS_send_and_recv_base
#undef SYS_poll
#undef SYS_wait

#ifdef __cplusplus
};
#else
#endif /* __cplusplus */
#endif /* OS_SYSCALLS */
