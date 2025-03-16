#ifndef OS_SYSCALLS
#define OS_SYSCALLS

#include <types.h>
#include <oslib.h>
#include <os/msg/message.h>

#define SYS_SEND		        		2u
#define SYS_RECV		        	  3u
#define SYS_SEND_AND_RECV       4u
#define SYS_GET_PAGE_MAPPINGS   5u
#define SYS_SET_PAGE_MAPPINGS   6u
#define SYS_CREATE_THREAD    		7u
#define SYS_DESTROY_THREAD	    8u
#define SYS_READ_THREAD		    	9u
#define SYS_UPDATE_THREAD	    	10u
#define SYS_POLL								11u
#define SYS_EOI									12u

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

#define ESYS_OK			0
#define ESYS_ARG		-1
#define ESYS_FAIL		-2
#define ESYS_PERM		-3
#define ESYS_BADCALL		-4
#define ESYS_NOTIMPL		-5
#define ESYS_NOTREADY         	-6
#define ESYS_INT              	-7

#define TF_STATUS		1u
#define TF_PRIORITY		2u
#define TF_REG_STATE		4u
#define TF_ROOT_PMAP		8u
#define TF_EXT_REG_STATE	16u
#define TF_CAP_TABLE		32u
#define TF_EX_HANDLER		64u
#define TF_EVENTS		128u

#define EV_STOP_CHILD	(1u << 0)
#define EV_STOP_PARENT 	(1u << 1)
#define EV_STOP_EH 	(1u << 2)
#define EV_STOP_PAGER 	(1u << 3)
#define EV_IRQ0 	(1u << 8)
#define EV_IRQ1 	(1u << 9)
#define EV_IRQ2 	(1u << 10)
#define EV_IRQ3 	(1u << 11)
#define EV_IRQ4 	(1u << 12)
#define EV_IRQ5 	(1u << 13)
#define EV_IRQ6 	(1u << 14)
#define EV_IRQ7 	(1u << 15)
#define EV_IRQ8 	(1u << 16)
#define EV_IRQ9 	(1u << 17)
#define EV_IRQ10 	(1u << 18)
#define EV_IRQ11 	(1u << 19)
#define EV_IRQ12 	(1u << 20)
#define EV_IRQ13 	(1u << 21)
#define EV_IRQ14 	(1u << 22)
#define EV_IRQ15 	(1u << 23)
#define EV_IRQ16 	(1u << 24)
#define EV_IRQ17 	(1u << 25)
#define EV_IRQ18 	(1u << 26)
#define EV_IRQ19 	(1u << 27)
#define EV_IRQ20 	(1u << 28)
#define EV_IRQ21 	(1u << 29)
#define EV_IRQ22 	(1u << 30)
#define EV_IRQ23 	(1u << 31)

typedef struct {
    void* send_buffer;
    void* recv_buffer;
    size_t send_length;
    size_t recv_length;
} SysMessageArgs;

struct PageMapping {
    union {
        pframe_t phys_frame;
        uint32_t block_num;
    };
    unsigned int flags;
};

struct LegacyXSaveState {
    uint16_t fcw;
    uint16_t fsw;
    uint8_t ftw;
    uint8_t _resd1;
    uint16_t fop;
    uint32_t fpuIp;
    uint16_t fpuCs;
    uint16_t _resd2;
    uint32_t fpuDp;
    uint16_t fpuDs;
    uint16_t _resd3;
    uint32_t mxcsr;
    uint32_t mxcsrMask;

    union {
        uint8_t mm0[10];
        uint8_t st0[10];
    };

    uint8_t _resd4[6];

    union {
        uint8_t mm1[10];
        uint8_t st1[10];
    };

    uint8_t _resd5[6];

    union {
        uint8_t mm2[10];
        uint8_t st2[10];
    };

    uint8_t _resd6[6];

    union {
        uint8_t mm3[10];
        uint8_t st3[10];
    };

    uint8_t _resd7[6];

    union {
        uint8_t mm4[10];
        uint8_t st4[10];
    };

    uint8_t _resd8[6];

    union {
        uint8_t mm5[10];
        uint8_t st5[10];
    };

    uint8_t _resd9[6];

    union {
        uint8_t mm6[10];
        uint8_t st6[10];
    };

    uint8_t _resd10[6];

    union {
        uint8_t mm7[10];
        uint8_t st7[10];
    };

    uint8_t _resd11[6];

    uint8_t xmm0[16];
    uint8_t xmm1[16];
    uint8_t xmm2[16];
    uint8_t xmm3[16];
    uint8_t xmm4[16];
    uint8_t xmm5[16];
    uint8_t xmm6[16];
    uint8_t xmm7[16];

    // Even though the area is reserved, the processor won't access the reserved bits in 32-bit mode

    uint8_t _resd12[176];
    uint8_t available[48];

};

typedef struct {
    struct State {
        uint32_t eax;
        uint32_t ebx;
        uint32_t ecx;
        uint32_t edx;
        uint32_t edi;
        uint32_t esi;
        uint32_t ebp;
        uint32_t esp;
        uint32_t eip;
        uint16_t cs;
        uint16_t ds;
        uint16_t es;
        uint16_t fs;
        uint16_t gs;
        uint16_t ss;
        uint32_t eflags;
    } state;

    uint32_t root_pmap;
    uint8_t status;
    uint8_t current_processor_id;
    tid_t tid;

    uint8_t priority;

    uint32_t pending_events;
    uint32_t event_mask;

    void* capability_table;
    size_t capability_table_len;

    tid_t exception_handler;
    tid_t pager;

    tid_t wait_tid;

    tid_t receiverWaitHead;
    tid_t receiverWaitTail;

    tid_t senderWaitHead;
    tid_t senderWaitTail;

    xsave_state_t* xsave_state;
    size_t xsave_state_len;
    uint16_t xsave_state_bits;

} thread_info_t;

#ifdef __cplusplus
extern "C" {
#else
#endif /* __cplusplus */

#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNU__MINOR__ >= 9)

#define SYSCALL1(syscallName, type1, arg1) \
static inline int sys_##syscallName(type1 arg1) { \
	int retVal; \
	asm volatile("pushl %%ebp\n" \
							 "pushl %%edx\n" \
							 "pushl %%ecx\n" \
							 "lea 1f, %%edx\n" \
							 "movl %%esp, %%ebp\n" \
							 "sysenter\n" \
							 "1:\n" \
							 "movl %%ebp, %%esp\n" \
							 "popl %%ecx\n" \
							 "popl %%edx\n" \
							 "popl %%ebp\n" \
									 : "=a"(retVal) \
									 : "a"(SYS_##syscallName), \
										 "b"(arg1) \
									 : "memory"); \
	return retVal; \
}

// Dummy variable has to be used because input-only arguments can't be modified

#define SYSCALL2(syscallName, type1, arg1, type2, arg2) \
static inline int sys_##syscallName(type1 arg1, type2 arg2) { \
	int retVal; \
	int dummy; \
	asm volatile("pushl %%ebp\n" \
							 "pushl %%edx\n" \
							 "pushl %%ecx\n" \
							 "lea 1f, %%edx\n" \
							 "movl %%esp, %%ebp\n" \
							 "sysenter\n" \
							 "1:\n" \
							 "movl %%ebp, %%esp\n" \
							 "popl %%ecx\n" \
							 "popl %%edx\n" \
							 "popl %%ebp\n" \
									 : "=a"(retVal), "=d"(dummy) \
									 : "a"(SYS_##syscallName), \
										 "b"(arg1), "c"(arg2) \
									 : "memory"); \
	return retVal; \
}

#define SYSCALL3(syscallName, type1, arg1, type2, arg2, type3, arg3) \
static inline int sys_##syscallName(type1 arg1, type2 arg2, type3 arg3) { \
	int retVal; \
	int dummy; \
	asm volatile("pushl %%ebp\n" \
							 "pushl %%edx\n" \
							 "pushl %%ecx\n" \
							 "lea 1f, %%edx\n" \
							 "movl %%esp, %%ebp\n" \
							 "sysenter\n" \
							 "1:\n" \
							 "movl %%ebp, %%esp\n" \
							 "popl %%ecx\n" \
							 "popl %%edx\n" \
							 "popl %%ebp\n" \
									: "=a"(retVal), "=d"(dummy) \
									: "a"(SYS_##syscallName), \
										"b"(arg1), "c"(arg2), "S"(arg3) \
									: "memory"); \
	return retVal; \
}

#define SYSCALL4(syscallName, type1, arg1, type2, arg2, type3, arg3, type4, arg4) \
static inline int sys_##syscallName(type1 arg1, type2 arg2, type3 arg3, type4 arg4) { \
    int retVal; \
    int dummy; \
	asm volatile("pushl %%ebp\n" \
        "pushl %%edx\n" \
        "pushl %%ecx\n" \
        "lea 1f, %%edx\n" \
        "movl %%esp, %%ebp\n" \
        "sysenter\n" \
        "1:\n" \
        "movl %%ebp, %%esp\n" \
        "popl %%ecx\n" \
        "popl %%edx\n" \
        "popl %%ebp\n" \
            : "=a"(retVal), "=d"(dummy) \
            : "a"(SYS_##syscallName), \
                "b"(arg1), "c"(arg2), "S"(arg3), "D"(arg4) \
            : "memory"); \
	return retVal; \
}

#define SYSCALL4_PACKED_W1(syscallName, type1, arg1, type2, arg2, type3, arg3, type4, arg4, wArg) \
static inline int sys_##syscallName(type1 arg1, type2 arg2, type3 arg3, type4 arg4, uint16_t wArg) { \
	int retVal; \
	int dummy; \
	asm volatile("pushl %%ebp\n" \
        "pushl %%edx\n" \
        "pushl %%ecx\n" \
        "lea 1f, %%edx\n" \
        "movl %%esp, %%ebp\n" \
        "sysenter\n" \
        "1:\n" \
        "movl %%ebp, %%esp\n" \
        "popl %%ecx\n" \
        "popl %%edx\n" \
        "popl %%ebp\n" \
            : "=a"(retVal), "=d"(dummy) \
            : "a"((((uint32_t)wArg << 16) & 0xFFFFu) | SYS_##syscallName), \
                "b"(arg1), "c"(arg2), "S"(arg3), "D"(arg4) \
            : "memory"); \
	return retVal; \
}
#else
#error "GCC 4.9 or greater is required."
#endif /* __GNUC__ > 4 || (__GNUC__ == 4 && __GNU__MINOR__ >= 9) */

#define SYS_send_and_recv 			SYS_SEND_AND_RECV
#define SYS_get_page_mappings 	SYS_GET_PAGE_MAPPINGS
#define SYS_set_page_mappings 	SYS_SET_PAGE_MAPPINGS
#define SYS_create_thread 			SYS_CREATE_THREAD
#define SYS_destroy_thread 			SYS_DESTROY_THREAD
#define SYS_read_thread 				SYS_READ_THREAD
#define SYS_update_thread 			SYS_UPDATE_THREAD
#define SYS_poll								SYS_POLL
#define SYS_eoi									SYS_EOI

#define SYS_get_page_mappings_base	SYS_get_page_mappings
#define SYS_set_page_mappings_base SYS_set_page_mappings
#define SYS_create_thread_base 		SYS_create_thread

SYSCALL4_PACKED_W1(get_page_mappings_base, addr_t, virt, size_t, count, uint32_t,
    addrSpace, struct PageMapping*, mappings, level)
SYSCALL4_PACKED_W1(set_page_mappings_base, addr_t, virt, size_t, count, uint32_t,
    addrSpace, struct PageMapping*, mappings, level)
SYSCALL3(create_thread_base, void*, entry, uint32_t, rootPmap, void*, stackTop)
SYSCALL2(read_thread, tid_t, tid, thread_info_t*, info)
SYSCALL3(update_thread, tid_t, tid, unsigned int, flags, thread_info_t*, info)
SYSCALL1(destroy_thread, tid_t, tid)
SYSCALL2(poll, int, mask, int, blocking)
SYSCALL1(eoi, int, mask)
SYSCALL4(send_and_recv, tid_t, recipient, uint32_t, subject,
    uint32_t, flags, SysMessageArgs*, args)
    
static inline tid_t sys_create_thread(void* entry, uint32_t rootPmap,
    void* stackTop)
{
    return (tid_t)sys_create_thread_base(entry, rootPmap, stackTop);
}

static inline int sys_get_page_mappings(unsigned int level, addr_t virt,
    size_t count, uint32_t addrSpace,
    struct PageMapping* mappings)
{
    return sys_get_page_mappings_base(virt, count, addrSpace, mappings, (uint16_t)level);
}

static inline int sys_set_page_mappings(unsigned int level, addr_t virt,
    size_t count, uint32_t addrSpace,
    struct PageMapping* mappings)
{
    return sys_set_page_mappings_base(virt, count, addrSpace, mappings, (uint16_t)level);
}

#undef SYS_send_and_recv
#undef SYS_get_page_mappings
#undef SYS_set_page_mappings
#undef SYS_create_thread
#undef SYS_destroy_thread
#undef SYS_read_thread
#undef SYS_update_thread
#undef SYS_get_page_mappings_base
#undef SYS_set_page_mappings_base
#undef SYS_create_thread_base
#undef SYS_poll
#undef SYS_eoi

#ifdef __cplusplus
};
#else
#endif /* __cplusplus */
#endif /* OS_SYSCALLS */
