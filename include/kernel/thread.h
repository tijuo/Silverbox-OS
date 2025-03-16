#ifndef KERNEL_THREAD_H
#define KERNEL_THREAD_H

#include <types.h>
#include <kernel/list_struct.h>
#include <kernel/mm.h>
#include <kernel/lowlevel.h>
#include <os/msg/message.h>
#include <util.h>

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

#define MAX_PROCESSORS   		16u

#define KERNEL_STACK_SIZE		2048

/* This assumes a uniprocessor system */

/* Touching the data types of this struct may break the context switcher! */
/// Contains the necessary data for a thread
struct ThreadControlBlock;
typedef struct ThreadControlBlock tcb_t;

struct ThreadControlBlock {
    uint8_t _padding;
    uint8_t wait_for_kernel_msg : 1;
    uint8_t thread_state : 4;
    uint8_t priority : 3;
    tid_t wait_tid;
    list_t receiver_wait_queue; // queue of threads waiting to receive a message from this thread
    list_t sender_wait_queue; // queue of threads waiting to send a message to this thread
    tid_t prev_tid;
    tid_t next_tid;

    // 16 bytes

    tid_t ex_handler; // Exception handler tid
    tid_t pager;      // Pager tid

    xsave_state_t* xsave_state;
    uint16_t xsave_state_len;
    uint16_t xsave_state_bits;

    uint32_t event_mask;

    // 32 bytes

    uint32_t pending_events;

    void* cap_table;
    size_t cap_table_size;

    uint8_t available[4];

    // 48 bytes

    uint8_t available2[16];

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

NON_NULL_PARAMS tcb_t* create_thread(void* entryAddr, uint32_t addrSpace,
    void* stackTop);

NON_NULL_PARAMS int start_thread(tcb_t* thread);
NON_NULL_PARAMS int pause_thread(tcb_t* thread);
NON_NULL_PARAMS int release_thread(tcb_t* thread);
NON_NULL_PARAMS void switch_context(tcb_t* thread, int doXSave);
NON_NULL_PARAMS int remove_thread_from_list(tcb_t* thread);
NON_NULL_PARAMS int wakeup_thread(tcb_t* thread);

extern tcb_t* init_server_thread;
extern tcb_t* init_pager_thread;
extern tcb_t tcb_table[MAX_THREADS];
extern ALIGNED(PAGE_SIZE) uint8_t kernel_stack[PAGE_SIZE];
extern uint8_t* kernel_stack_top;

static inline CONST proc_id_t get_current_processor(void)
{
    return 0;
}

/** Retrieve the thread executing on the current processor (the one from which this function is called). 
 * @return The current thread that's running on this processor.
 */
static inline tcb_t* get_current_thread(void)
{
    return processors[get_current_processor()].running_thread;
}

/**
 * Sets the current thread for this processor.
 * @param tcb The thread to set for this processor
 */
static inline void set_current_thread(tcb_t* tcb)
{
    processors[get_current_processor()].running_thread = tcb;
}

NON_NULL_PARAMS static inline tid_t get_tid(tcb_t* tcb)
{
    return (tid_t)(tcb - tcb_table);
}

static inline tcb_t* get_tcb(tid_t tid)
{
    return tid == NULL_TID ? NULL : &tcb_table[tid];
}

#endif /* KERNEL_THREAD_H */
