#include <kernel/debug.h>
#include <kernel/thread.h>
#include <kernel/mm.h>
#include <util.h>
#include <kernel/error.h>
#include <kernel/memory.h>

list_t blocked_list;
list_t zombie_list;

struct Processor processors[MAX_PROCESSORS];
size_t num_processors;

static tid_t generate_new_tid(void);

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
      list_remove(&blocked_list, thread);
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
 * @param thread The thread to be blocked.
 * @return E_OK on success. E_FAIL on failure. E_DONE if thread is already blocked.
 */

NON_NULL_PARAMS int block_thread(tcb_t *thread) {
  switch(thread->thread_state) {
    case READY:
    case RUNNING:
      remove_thread_from_list(thread);
      break;
    case BLOCKED:
      RET_MSG(E_DONE, "Thread is already blocked.");
    default:
      RET_MSG(E_FAIL, "Thread cannot be blocked.");
  }

  thread->thread_state = BLOCKED;
  list_enqueue(&blocked_list, thread);

  return E_OK;
}

/**
 Creates and initializes a new thread.

 @param pcb The PCB of the process for which the thread will
 be added.
 @param entry_addr The address at which the thread's instruction
 pointer will be set.
 @param stack_top The address at which the thread's stack
 pointer will be set.
 @return The TCB of the newly created thread on success. NULL, on failure.
 */

NON_NULL_PARAM(1) tcb_t* create_thread(pcb_t *pcb, void *entry_addr,
                                    void *stack_top)
{
  tid_t tid = generate_new_tid();

  if(tid == INVALID_TID)
    RET_MSG(NULL, "Unable to create a new thread id.");

  tcb_t thread = kcalloc(1, sizeof(tcb_t));

  if(thread == NULL)
    RET_MSG(NULL, "Unable to create a new thread");

  if(IS_ERROR(vector_push_back(&pcb->threads, thread))) {
    kfree(thread);
    RET_MSG(NULL, "Unable to add thread to process.");
  }

  exec_state_t *exec_state = (exec_state_t *)stack_top - 1;

  memset(exec_state, 0, sizeof *exec_state);

  exec_state->rflags = RFLAGS_IF;
  exec_state->rip = (uint64_t)entry_addr;
  exec_state->rsp = (uint64_t)stack_top;
  exec_state->cs = UCODE_SEL;
  exec_state->ds = UDATA_SEL;
  exec_state->es = UDATA_SEL;
  exec_state->ss = UDATA_SEL;

  thread->kernel_stack = stack_top;
  thread->priority = NORMAL_PRIORITY;

  if(IS_ERROR(list_enqueue(&blocked_list, thread))) {
    kfree(thread);
    RET_MSG(NULL, "Unable to enqueue thread to paused list.");
  }

  thread->pcb = pcb;
  thread->thread_state = WAITING;

  return thread;
}

/**
 Destroys a thread and detaches it from all queues.

 @param thread The TCB of the thread to release.
 @return E_OK on success. E_FAIL on failure.
 */

NON_NULL_PARAMS int release_thread(tcb_t *thread) {
  if(IS_ERROR(remove_thread_from_list(thread)))
    RET_MSG(E_FAIL, "Unable to remove thread from list");

  return IS_ERROR(vector_remove_item(&thread->pcb->threads, thread)) ? E_FAIL : E_OK;
}

/**
 * Generate a new TID for a thread.
 *
 * @return A TID that is available for use. INVALID_TID if no TIDs are available to be allocated.
 */

tid_t generate_new_tid(void) {
  static tid_t last_tid = 0;
  tid_t prev_tid = last_tid;

  do {
    if(last_tid == INVALID_TID)
      last_tid = 0;

    for(size_t i=0; i < vector_get_count(&process_vector); i++) {
      pcb_t *pcb = (pcb_t *)VECTOR_ITEM(&process_vector, i);

      for(size_t j=0; j < vector_get_count(&pcb->threads); j++) {
        tid_t *tcb = (tid_t)VECTOR_ITEM(&pcb->threads, j);

        if(tcb->tid == last_tid)
          goto continue_tid_search;
      }
    }

    return (tid_t)last_tid++;
continue_tid_search:
  } while(++last_tid != prev_tid);

  RET_MSG(INVALID_TID, "No more TIDs are available");
}

NON_NULL_PARAMS void switch_context(tcb_t *thread) {
  if((thread->pcb->root_pmap & CR3_BASE_MASK) != (get_cr3() & CR3_BASE_MASK))
    set_cr3(thread->pcb->root_pmap);

  tcb_t *current_thread = get_current_thread();

  if(current_thread) {
    if(current_thread->xsave_state)
      __asm__("fxsave  %0\n" :: "m"(current_thread->xsave_state));
  }

  if(thread->xsave_state)
	__asm__("fxrstor %0\n" :: "m"(thread->xsave_state));

//  activateContinuation(thread); // doesn't return if successful

// Restore user state

  tss.rsp0 = (uint64_t)((exec_state_t*)kernel_stack_top - 1) - sizeof(uint64_t);
  uint64_t *s = (uint64_t*)tss.rsp0;
  exec_state_t *state = (exec_state_t*)(s + 1);

  *s = (uint64_t)kernel_stack_top;
  *state = thread->exec_state;

  set_cr3(init_server_thread->root_pmap);
  RESTORE_STATE;
}
