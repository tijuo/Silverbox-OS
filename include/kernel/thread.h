#ifndef KERNEL_THREAD_H
#define KERNEL_THREAD_H

#include <types.h>
#include <kernel/list_struct.h>
#include <kernel/mm.h>
#include <kernel/lowlevel.h>
#include <os/msg/message.h>
#include <util.h>
#include <kernel/lock.h>

#define STACK_MAGIC        0xA74D762Eu

#define MAX_THREADS			    65536

// Thread is either uninitialized or destroyed.
#define INACTIVE		    	0

// Thread is in an infinite blocking state.
#define PAUSED			    	1

// Thread is waiting to be released.
#define ZOMBIE                  2 

// Thread is ready to be scheduled to a processor.
#define READY			    	3

// Thread is already scheduled to a processor.
#define RUNNING			    	4

// Thread is waiting for another thread to send it a message.
#define WAIT_FOR_SEND			5

// Thread is waiting for another thread to receive its message.
#define WAIT_FOR_RECV			6

// Thread is temporarily blocked.
#define SLEEPING				7

#define MAX_PROCESSORS   		64u

#define KERNEL_STACK_SIZE	    4096

_Static_assert(KERNEL_STACK_SIZE % PAGE_SIZE == 0, "Kernel stack size must be divisible by 4 KiB.");

/* This assumes a uniprocessor system */

/* Touching the data types of this struct may break the context switcher! */
/// Contains the necessary data for a thread
struct ThreadControlBlock;
typedef struct ThreadControlBlock tcb_t;

struct ThreadControlBlock {
    uint8_t _padding;
    /*
        * If `thread_state` is `WAIT_FOR_SEND`, then this bit indicates that the
        thread is waiting to receive a kernel message (and ignoring non-kernel messages).

        * If `thread_state` is `WAIT_FOR_RECV`, then this bit indicates that the thread
        is attempting to send a kernel message.
    */
    uint8_t wait_for_kernel_msg : 1;
    uint8_t thread_state : 4;
    uint8_t priority : 3;

    /* 
        * If `thread_state` is `WAIT_FOR_SEND`, then this indicates that this thread is waiting
        to receive a non-kernel message from `wait_tid`'s thread.

        * If `thread_state` is `WAIT_FOR_RECV`, then this indicates that this thread is waiting
        to send a message (it may be a kernel message) to `wait_tid`'s thread.
    */
    tid_t wait_tid;
    list_t receiver_wait_queue; // Queue of threads waiting to receive a message from this thread
    list_t sender_wait_queue; // Queue of threads waiting to send a message to this thread
    tid_t prev_tid;
    tid_t next_tid;

    // 16 bytes

    tid_t ex_handler; // Exception handler tid
    tid_t pager;      // Pager tid

    uint32_t event_mask;
    uint32_t pending_events;
    uint8_t available[4];

    // 32 bytes

    void* cap_table;
    size_t cap_table_size;

    uint8_t available2[8];

    // 48 bytes

    fxsave_state_t* fxsave_state;
    size_t fxsave_state_len;
    uint64_t xsave_rfbm;    // xsave requested feature bitmap

    // 64 bytes

    uint8_t available3[4];

    ExecutionState user_exec_state;
    uint32_t root_pmap;
};

struct Processor {
    bool is_online;
    uint8_t acpi_uid;
    uint8_t lapic_id;
    tcb_t* running_thread;
};

typedef uint8_t proc_id_t;
extern size_t num_processors;

_Static_assert(((int)sizeof(tcb_t) & -(int)sizeof(tcb_t)) == (int)sizeof(tcb_t),
    "TCB size is not properly aligned");

extern struct Processor processors[MAX_PROCESSORS];

WARN_UNUSED NON_NULL_PARAMS tcb_t* thread_create(void* entryAddr, uint32_t addr_space,
    void* stackTop);

    WARN_UNUSED NON_NULL_PARAMS int thread_start(tcb_t* thread);
WARN_UNUSED NON_NULL_PARAMS int thread_pause(tcb_t* thread);
WARN_UNUSED NON_NULL_PARAMS int thread_release(tcb_t* thread);
NON_NULL_PARAMS void thread_switch_context(tcb_t* thread, bool do_x_save);
WARN_UNUSED NON_NULL_PARAMS int thread_remove_from_list(tcb_t* thread);
WARN_UNUSED NON_NULL_PARAMS int thread_wakeup(tcb_t* thread);
WARN_UNUSED NON_NULL_PARAMS int thread_sleep(tcb_t *thread, unsigned int duration, int granularity);

extern tcb_t* init_server_thread;
extern tcb_t* init_pager_thread;
extern ALIGN_AS(PAGE_SIZE) uint8_t kernel_stack[KERNEL_STACK_SIZE];
extern uint8_t* kernel_stack_top;

WARN_UNUSED static inline CONST proc_id_t processor_get_current(void)
{
    return 0;
}

/** Retrieve the thread executing on the current processor (the one from which this function is called). 
 * @return The current thread that's running on this processor.
 */
WARN_UNUSED static inline tcb_t* thread_get_current(void)
{
    return processors[processor_get_current()].running_thread;
}

/**
 * Sets the current thread for this processor.
 * @param tcb The thread to set for this processor
 */
static inline void thread_set_current(tcb_t* tcb)
{
    processors[processor_get_current()].running_thread = tcb;
}

WARN_UNUSED NON_NULL_PARAMS tid_t get_tid(tcb_t* tcb);
WARN_UNUSED tcb_t* get_tcb(tid_t tid);

#endif /* KERNEL_THREAD_H */
