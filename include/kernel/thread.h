#ifndef KERNEL_THREAD_H
#define KERNEL_THREAD_H

#include <types.h>
#include <kernel/mm.h>
#include <kernel/exec_state.h>
#include <util.h>
#include <kernel/process.h>

enum thread_state {
  /// Thread has not been initialized. Fields are not in a valid state.
  INACTIVE,

  /// Waiting for I/O completion, can only be woken by kernel.
  BLOCKED,

  /// Thread is ready to be scheduled to a processor.
  READY,

  /// Thread is currently executing instructions.
  RUNNING,

  /// Thread is blocked, waiting for it to be woken.
  WAITING,

  /// Thread has stopped executing and is waiting to be released.
  ZOMBIE
};

#define MAX_PROCESSORS   		16u

#define KERNEL_STACK_SIZE		8192

/// Contains the necessary data for a thread

typedef struct {
  void *kernel_stack;

  /**
      Pointer to the thread's XSAVE structure.
      If NULL, then there's no XSAVE state that needs to be saved/restored.
  */
  xsave_state_t *xsave_state;

  /// The fields for which XSAVE should operate on
  uint64_t xsave_state_flags;
  pcb_t *pcb;

  tid_t tid;
  uint8_t priority;
  uint8_t state;

  uint8_t _padding[26];
} tcb_t;

struct processor {
  tcb_t *running_thread;
  bool is_online;
  uint8_t acpi_uid;
  uint8_t local_apic_id;
};

typedef uint8_t proc_id_t;
extern size_t num_processors;

_Static_assert(sizeof(tcb_t) == 32 || sizeof(tcb_t) == 64 || sizeof(tcb_t) == 128
               || sizeof(tcb_t) == 256 || sizeof(tcb_t) == 512 || sizeof(tcb_t) == 1024
               || sizeof(tcb_t) == 2048 || sizeof(tcb_t) == 4096,
               "TCB size is not properly aligned");

extern struct processor processors[MAX_PROCESSORS];

NON_NULL_PARAM(1) tcb_t* create_thread(pcb_t *pcb, void *entryAddr,
                                       void *stackTop);

NON_NULL_PARAMS int start_thread(tcb_t *thread);
NON_NULL_PARAMS int block_thread(tcb_t *thread);
NON_NULL_PARAMS int release_thread(tcb_t *thread);
NON_NULL_PARAMS void switch_context(tcb_t *thread);
NON_NULL_PARAMS int remove_thread_from_list(tcb_t *thread);
NON_NULL_PARAMS int wakeup_thread(tcb_t *thread);

static inline CONST unsigned int get_current_processor(void) {
  return 0;
}

/// @return The current thread that's running on this processor.

static inline PURE tcb_t* get_current_thread(void) {
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
