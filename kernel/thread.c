#include <kernel/debug.h>
#include <string.h>
#include <kernel/thread.h>
#include <kernel/schedule.h>
#include <kernel/mm.h>
#include <util.h>
#include <oslib.h>
#include <kernel/message.h>
#include <kernel/error.h>
#include <kernel/lowlevel.h>
#include <kernel/paging.h>
#include <kernel/interrupt.h>
#include <kernel/memory.h>

#define TID_START           0u

ALIGNED(PAGE_SIZE) uint8_t kernel_stack[PAGE_SIZE]; // The single kernel stack used by all threads (assumes a uniprocessor system)
uint8_t *kernel_stack_top = kernel_stack + PAGE_SIZE;

SECTION(".tcb") tcb_t tcb_table[MAX_THREADS];
tcb_t *init_server_thread;

list_t paused_list;
list_t zombie_list;

struct Processor processors[MAX_PROCESSORS];
size_t num_processors;

static tid_t get_new_tid(void);

NON_NULL_PARAMS int wakeup_thread(tcb_t *thread) {
  switch(thread->thread_state) {
    case RUNNING:
    case READY:
      RET_MSG(E_DONE, "Thread is already awake.");
    case ZOMBIE:
    case INACTIVE:
      RET_MSG(E_FAIL, "Unable to wake up inactive thread.");
    default:
      remove_thread_from_list(thread);
      thread->thread_state = READY;
      list_enqueue(&run_queues[thread->priority], thread);
      break;
  }

  return E_OK;
}

NON_NULL_PARAMS int remove_thread_from_list(tcb_t *thread) {
  switch(thread->thread_state) {
    case WAIT_FOR_RECV:
      detach_send_wait_queue(thread);
      thread->wait_tid = NULL_TID;
      break;
    case WAIT_FOR_SEND:
      detach_receive_wait_queue(thread);
      thread->wait_tid = NULL_TID;
      break;
    case READY:
      list_remove(&run_queues[thread->priority], thread);
      break;
    case PAUSED:
      list_remove(&paused_list, thread);
      break;
    case ZOMBIE:
      list_remove(&zombie_list, thread);
      break;
    case RUNNING:
      for(unsigned int processor_id = 0; processor_id < num_processors;
          processor_id++)
      {
        if(processors[processor_id].running_thread == thread) {
          processors[processor_id].running_thread = NULL;
          break;
        }
      }
      break;
    default:
      RET_MSG(E_FAIL, "Unable to remove thread from list.");
  }

  return E_OK;
}

/** Starts a non-running thread by placing it on a run queue.

 @param thread The thread to be started.
 @return E_OK on success. E_FAIL on failure. E_DONE if the thread is already started.
 */
NON_NULL_PARAMS int start_thread(tcb_t *thread) {
  switch(thread->thread_state) {
    case WAIT_FOR_RECV:
    case WAIT_FOR_SEND:
    case PAUSED:
      remove_thread_from_list(thread);
      break;
    case READY:
      RET_MSG(E_DONE, "Thread has already started.");
    case RUNNING:
      RET_MSG(E_DONE, "Thread is already running.");
    default:
      RET_MSG(E_FAIL, "Unable to start thread.");
  }

  thread->thread_state = READY;
  list_enqueue(&run_queues[thread->priority], thread);

  return E_OK;
}

/** Block a thread indefinitely until it is restarted.
 * @param thread The thread to be paused.
 * @return E_OK on success. E_FAIL on failure. E_DONE if thread is already paused.
 */

NON_NULL_PARAMS int pause_thread(tcb_t *thread) {
  switch(thread->thread_state) {
    case WAIT_FOR_RECV:
    case WAIT_FOR_SEND:
    case READY:
    case RUNNING:
      remove_thread_from_list(thread);
      break;
    case PAUSED:
      RET_MSG(E_DONE, "Thread is already paused.");
    default:
      RET_MSG(E_FAIL, "Thread cannot be paused.");
  }

  thread->thread_state = PAUSED;
  list_enqueue(&paused_list, thread);

  return E_OK;
}

/**
 Creates and initializes a new thread.

 @param entry_addr The start address of the thread.
 @param addr_space The physical address of the thread's page directory.
 @param stack_top The address of the top of the thread's stack.
 @return The TCB of the newly created thread. NULL on failure.
 */

NON_NULL_PARAMS tcb_t* create_thread(void *entry_addr, addr_t addr_space,
                                    void *stack_top)
{
  tcb_t *thread = NULL;
  uint32_t root_pmap =
      addr_space == CURRENT_ROOT_PMAP ? get_root_page_map() : addr_space;

  if(IS_ERROR(initialize_root_pmap(root_pmap)))
    RET_MSG(NULL, "Unable to initialize page map.");

  tid_t tid = get_new_tid();

  if(tid == NULL_TID)
    RET_MSG(NULL, "Unable to create a new thread id.");

  thread = get_tcb(tid);

  if(thread->thread_state != INACTIVE)
    RET_MSG(NULL, "Thread is already active.");

  memset(thread, 0, sizeof(tcb_t));
  thread->root_pmap = (dword)root_pmap;

  thread->user_exec_state.eflags = EFLAGS_IOPL3 | EFLAGS_IF;
  thread->user_exec_state.eip = (dword)entry_addr;

  thread->user_exec_state.user_esp = (dword)stack_top;
  thread->user_exec_state.cs = UCODE_SEL;
  thread->user_exec_state.ds = UDATA_SEL;
  thread->user_exec_state.es = UDATA_SEL;
  thread->user_exec_state.user_ss = UDATA_SEL;

  thread->priority = NORMAL_PRIORITY;

  thread->children_head = NULL_TID;

  kprintf("Created new thread at %#p (tid: %u, pmap: %#p)\n", thread, tid,
          (void*)(uintptr_t)thread->root_pmap);

  thread->thread_state = PAUSED;
  list_enqueue(&paused_list, thread);

  tcb_t *current_thread = get_current_thread();

  if(current_thread) {
    thread->parent = get_tid(current_thread);
    thread->next_sibling = current_thread->children_head;
    current_thread->children_head = tid;
  }
  else {
    thread->parent = NULL_TID;
    thread->next_sibling = NULL_TID;
  }
  return thread;
}

/**
 Destroys a thread and detaches it from all queues.

 @param thread The TCB of the thread to release.
 @return E_OK on success. E_FAIL on failure.
 */

NON_NULL_PARAMS int release_thread(tcb_t *thread) {
  list_t *queues[2] = {
    &thread->sender_wait_queue,
    &thread->receiver_wait_queue
  };
  list_t *queue;
  size_t i;

  /* If the thread is a sender waiting for a recipient, remove the
   sender from the recipient's wait queue. If the thread is a
   receiver, clear its wait queue after waking all of the waiting
   threads. */

  switch(thread->thread_state) {
    case INACTIVE:
      RET_MSG(E_DONE, "Thread is already inactive.");
    default:
      remove_thread_from_list(thread);
  }

  /* Notify any threads waiting for a send/receive that the operation
   failed. */

  for(i = 0, queue = queues[0]; i < ARRAY_SIZE(queues); i++, queue++) {
    while(queue->head_tid != NULL_TID) {
      tcb_t *t = list_dequeue(queue);

      if(t) {
        t->wait_tid = NULL_TID;
        t->user_exec_state.eax = E_UNREACH;
        kprintf("Releasing thread. Starting %u\n", get_tid(t));
        t->thread_state = READY;
      }
    }
  }

  // Unregister the thread as an exception handler

  for(i = 0; i < NUM_IRQS; i++) {
    if(irq_handlers[i] == thread)
      irq_handlers[i] = NULL_TID;
  }

  // Release any zombie child threads

  for(tid_t tid = thread->children_head; tid != NULL_TID;) {
    tcb_t *tcb = get_tcb(tid);

    if(tcb->thread_state == ZOMBIE)
      release_thread(tcb);
    else
      tcb->parent = NULL_TID;

    tid = tcb->next_sibling;
  }

  thread->thread_state = INACTIVE;
  return E_OK;
}

/**
 * Generate a new TID for a thread.
 *
 * @return A TID that is available for use. NULL_TID if no TIDs are available to be allocated.
 */

tid_t get_new_tid(void) {
  static int last_tid = TID_START;
  int prev_tid = last_tid++;

  do {
    if(last_tid == MAX_THREADS)
      last_tid = TID_START;
  } while(last_tid != prev_tid && get_tcb(last_tid)->thread_state != INACTIVE);

  if(last_tid == prev_tid)
    RET_MSG(NULL_TID, "No more TIDs are available");

  return (tid_t)last_tid;
}

NON_NULL_PARAMS void switch_context(tcb_t *thread, int do_xsave) {
  assert(thread->thread_state == RUNNING);

  if((thread->root_pmap & CR3_BASE_MASK) != (get_cr3() & CR3_BASE_MASK))
    set_cr3(thread->root_pmap);

  tcb_t *current_thread = get_current_thread();

  if(current_thread) {
    if(do_xsave)
      __asm__("fxsave  %0\n" :: "m"(current_thread->xsave_state));
  }

  __asm__("fxrstor %0\n" :: "m"(thread->xsave_state));

//  activateContinuation(thread); // doesn't return if successful

// Restore user state

  tss.esp0 = (uint32_t)((ExecutionState*)kernel_stack_top - 1) - sizeof(uint32_t);
  uint32_t *s = (uint32_t*)tss.esp0;
  ExecutionState *state = (ExecutionState*)(s + 1);

  *s = (uint32_t)kernel_stack_top;
  *state = thread->user_exec_state;

  set_cr3(init_server_thread->root_pmap);
  RESTORE_STATE;
}
