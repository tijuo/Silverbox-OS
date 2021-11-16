#ifndef KERNEL_THREAD_H
#define KERNEL_THREAD_H

#include <types.h>
#include <kernel/list_struct.h>
#include <kernel/mm.h>
#include <kernel/lowlevel.h>
#include <os/msg/message.h>
#include <util.h>

#define STACK_MAGIC             0xA74D762E

#define MAX_THREADS			    65536

#define INACTIVE		    	0
#define PAUSED			    	1  // Infinite blocking state
#define ZOMBIE                  2  // Thread is waiting to be released
#define READY			    	3  // Thread is ready to be scheduled to a processor
#define RUNNING			    	4  // Thread is already scheduled to a processor
#define WAIT_FOR_SEND			5
#define WAIT_FOR_RECV			6

#define MAX_PROCESSORS   		16u

#define KERNEL_STACK_SIZE		2048

#define getTid(tcb)			({ __typeof__ (tcb) _tcb=(tcb); (_tcb ? (tid_t)(_tcb - tcbTable) : NULL_TID); })
#define getTcb(tid)			({ __typeof__ (tid) _tid=(tid); (_tid == NULL_TID ? NULL : &tcbTable[_tid]); })

/* This assumes a uniprocessor system */

/* Touching the data types of this struct may break the context switcher! */
/// Contains the necessary data for a thread
struct ThreadControlBlock;
typedef struct ThreadControlBlock tcb_t;
//typedef void (*finish_t)(tcb_t *);

struct ThreadControlBlock {
  uint8_t _padding;
  uint8_t waitForKernelMsg :1;
  uint8_t threadState :4;
  uint8_t priority :3;
  tid_t waitTid;
  list_t receiverWaitQueue; // queue of threads waiting to receive a message from this thread
  list_t senderWaitQueue; // queue of threads waiting to send a message to this thread
  tid_t prevTid;
  tid_t nextTid;

  tid_t exHandler;
  tid_t pager;

  tid_t parent;
  tid_t childrenHead;
  tid_t nextSibling;

  uint8_t available[30];

  void *capTable;
  size_t capTableSize;

  // 64 bytes

  xsave_state_t xsaveState;

  uint8_t available2[32];

  // 384 bytes

  uint8_t available3[68];

  ExecutionState userExecState;
  uint32_t rootPageMap;
};

struct Processor {
  bool isOnline;
  uint8_t acpiUid;
  uint8_t lapicId;
  tcb_t *runningThread;
};

typedef uint8_t proc_id_t;
extern size_t numProcessors;

_Static_assert(sizeof(tcb_t) == 32 || sizeof(tcb_t) == 64 || sizeof(tcb_t) == 128
               || sizeof(tcb_t) == 256 || sizeof(tcb_t) == 512 || sizeof(tcb_t) == 1024
               || sizeof(tcb_t) == 2048 || sizeof(tcb_t) == 4096,
               "TCB size is not properly aligned");

extern struct Processor processors[MAX_PROCESSORS];

NON_NULL_PARAMS tcb_t* createThread(void *entryAddr, uint32_t addrSpace,
                                    void *stackTop);

NON_NULL_PARAMS int startThread(tcb_t *thread);
NON_NULL_PARAMS int pauseThread(tcb_t *thread);
NON_NULL_PARAMS int releaseThread(tcb_t *thread);
NON_NULL_PARAMS void switchContext(tcb_t *thread, int doXSave);
NON_NULL_PARAMS int removeThreadFromList(tcb_t *thread);
NON_NULL_PARAMS int wakeupThread(tcb_t *thread);

extern tcb_t *initServerThread;
extern tcb_t *initPagerThread;
extern tcb_t tcbTable[MAX_THREADS];
extern ALIGNED(PAGE_SIZE) uint8_t kernelStack[PAGE_SIZE];
extern uint8_t *kernelStackTop;

/*
 static inline void activateContinuation(tcb_t *thread)
 {
 if((uintptr_t)thread->continuationStackTop < ((uintptr_t)thread->continuationStack + sizeof thread->continuationStack))
 {
 uint32_t *ptr = (uint32_t *)tssEsp0;

 *--ptr = (uint32_t)thread;
 *--ptr = (uint32_t)*thread->continuationStackTop++;

 __asm__("lea %0, %%esp\n"
 "jmp %%esp\n" :: "m"(ptr));
 }
 }

 static inline void saveContinuation(tcb_t *thread, finish_t finishFunc)
 {
 if(thread->continuationStackTop != thread->continuationStack)
 *--thread->continuationStackTop = finishFunc;
 }
 */

static inline CONST unsigned int getCurrentProcessor(void) {
  return 0;
}

/// @return The current thread that's running on this processor.

static inline tcb_t* getCurrentThread(void) {
  return processors[getCurrentProcessor()].runningThread;
}

/**
 * Sets the current thread for this processor.
 * @param tcb The thread to set for this processor
 */

static inline void setCurrentThread(tcb_t *tcb) {
  processors[getCurrentProcessor()].runningThread = tcb;
}

#endif /* KERNEL_THREAD_H */
