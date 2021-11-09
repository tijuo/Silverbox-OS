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

struct ThreadControlBlock
{
  dword rootPageMap;
  unsigned char _padding;
  unsigned char waitForKernelMsg : 1;
  unsigned char threadState : 4;
  unsigned char priority : 3;
  tid_t waitTid;
  list_t receiverWaitQueue; // queue of threads waiting to receive a message from this thread
  list_t senderWaitQueue;   // queue of threads waiting to send a message to this thread

  // 16-byte offset

  ExecutionState userExecState;

  // 64-byte offset

  tid_t prevTid;
  tid_t nextTid;

  tid_t exHandler;
  tid_t pager;

  uint8_t available[24];
  xsave_state_t xsaveState;

  // 384-byte offset

  uint8_t available2[128];
} PACKED;

tcb_t *createThread(void *entryAddr, uint32_t addrSpace, void *stackTop);

int startThread( tcb_t *thread );
int pauseThread( tcb_t *thread );
int releaseThread( tcb_t *thread );
void switchContext(tcb_t *thread, int doXSave);
int removeThreadFromList(tcb_t *thread);

extern tcb_t *initServerThread;
extern tcb_t *initPagerThread;
extern tcb_t tcbTable[MAX_THREADS];
extern tcb_t *runningThreads[MAX_PROCESSORS];

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

static inline CONST unsigned int getCurrentProcessor(void)
{
  return 0;
}

/// @return The current thread that's running on this processor.

static inline tcb_t *getCurrentThread(void)
{
  return runningThreads[getCurrentProcessor()];
}

/**
 * Sets the current thread for this processor.
 * @param tcb The thread to set for this processor
 */

static inline void setCurrentThread(tcb_t *tcb)
{
  runningThreads[getCurrentProcessor()] = tcb;
}

#define RESTORE_STATE \
__asm__( \
        "call switchStacks\n"       \
        "add  $4, %esp\n"           \
                                    \
        "testw $0x03, 36(%esp)\n"   \
        "jz  1f\n"                  \
                                    \
        "mov   $0x23, %ax\n"        \
        "mov   %ax, %ds\n"          \
        "mov   %ax, %es\n"          \
                                    \
        "1:\n"                      \
        "pop %edi\n"                \
        "pop %esi\n"                \
        "pop %ebp\n"                \
        "pop %ebx\n"                \
        "pop %edx\n"                \
        "pop %ecx\n"                \
        "pop %eax\n"                \
  )

#endif /* KERNEL_THREAD_H */
