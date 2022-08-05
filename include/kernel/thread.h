#ifndef KERNEL_THREAD_H
#define KERNEL_THREAD_H

#include <assert.h>
#include <types.h>
#include <kernel/list_struct.h>
#include <kernel/mm.h>
#include <kernel/lowlevel.h>
#include <os/msg/message.h>
#include <util.h>

#define MAX_THREADS			    65536

#define INACTIVE		    	0
#define PAUSED			    	1  // Infinite blocking state
#define ZOMBIE                  2  // Thread is waiting to be released
#define READY			    	3  // Thread is ready to be scheduled to a processor
#define RUNNING			    	4  // Thread is already scheduled to a processor
#define WAIT_FOR_SEND			5
#define WAIT_FOR_RECV			6
#define SLEEPING				7

#define MAX_PROCESSORS   		16u

#define KERNEL_STACK_SIZE		2048

#define get_tid(tcb)			({ __typeof__ (tcb) _tcb=(tcb); (_tcb ? (tid_t)(_tcb - tcb_table) : NULL_TID); })
#define get_tcb(tid)			({ __typeof__ (tid) _tid=(tid); (_tid == NULL_TID ? NULL : &tcb_table[_tid]); })

/* This assumes a uniprocessor system */

/* Touching the data types of this struct may break the context switcher! */

struct cap_entry;

/**
  Contains the necessary data for a thread.
*/

typedef struct ThreadControlBlock {
  uint64_t root_pmap; // The physical address of the top-level page map structure (typically PML4).
  uint8_t priority;
  uint8_t wait_for_kernel_msg :1;
  uint8_t thread_state :4;
  tid_t wait_tid;
  list_t receiver_wait_queue; // queue of threads waiting to receive a message from this thread
  list_t sender_wait_queue; // queue of threads waiting to send a message to this thread
  tid_t prev_tid;
  tid_t next_tid;

  tid_t ex_handler;
  tid_t pager;

  tid_t parent;
  tid_t children_head;
  tid_t next_sibling;

  uint8_t available[8]; // unused padding to fill 64 B cache line
  cap_index_t cap_entry_head;
  cap_index_t cap_free_head;
  uint16_t cap_table_capacity;

  uint32_t event_mask;
  uint32_t pending_events;

  struct cap_entry *cap_table;

  // 64 bytes


  // 384 bytes

  uint8_t available2[16]; // unused padding to fill 64 B cache line

  xsave_state_t *xsave_state;
  exec_state_t user_exec_state;
} tcb_t;

struct Processor {
  tcb_t *running_thread;
  bool is_online;
  uint8_t acpi_uid;
  uint8_t lapic_id;
};

typedef uint8_t proc_id_t;
extern size_t num_processors;

static_assert(IS_POWER_OF_TWO(sizeof(tcb_t)), "TCB size must be a power of 2.");

extern struct Processor processors[MAX_PROCESSORS];

NON_NULL_PARAMS tcb_t* create_thread(void *entry_addr, paddr_t addr_space,
                                     void *stack_top);

NON_NULL_PARAMS int start_thread(tcb_t *thread);
NON_NULL_PARAMS int pause_thread(tcb_t *thread);
NON_NULL_PARAMS int release_thread(tcb_t *thread);
NON_NULL_PARAMS void switch_context(tcb_t *thread);
NON_NULL_PARAMS int remove_thread_from_list(tcb_t *thread);
NON_NULL_PARAMS int wakeup_thread(tcb_t *thread);

extern tcb_t *init_server_thread;
extern tcb_t *init_pager_thread;
extern tcb_t tcb_table[MAX_THREADS];
extern ALIGNED(PAGE_SIZE) uint8_t kernel_stack[4*PAGE_SIZE];
extern uint8_t *kernel_stack_top;

/// TODO: To be implemented.

static inline CONST unsigned int get_current_processor(void) {
  return 0;
}

/// @return The current thread that's running on this processor.

static inline tcb_t* get_current_thread(void) {
  return processors[get_current_processor()].running_thread;
}

/**
 * Sets the current thread for this processor.
 * @param tcb The thread to set for this processor
 */

static inline void set_current_thread(tcb_t *tcb) {
  processors[get_current_processor()].running_thread = tcb;
}

#endif /* KERNEL_THREAD_H */
