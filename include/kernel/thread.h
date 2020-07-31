#ifndef THREAD_H
#define THREAD_H

#include <types.h>
#include <kernel/mm.h>
#include <kernel/lowlevel.h>
#include <kernel/struct.h>
#include <kernel/message.h>

#define STOPPED			0
#define PAUSED			1  // Infinite blocking state
#define SLEEPING		2  // Blocks until timer runs out
#define READY			3  // <-- Do not touch(or the context switch code will break)
				   // Thread is ready to be scheduled to a processor
#define RUNNING			4  // <-- Do not touch(or the context switch code will break)
				   // Thread is already scheduled to a processor
#define WAIT_FOR_SEND		5
#define WAIT_FOR_RECV		6
#define ZOMBIE			7  // Thread is waiting to be released
#define IRQ_WAIT		8

#define NUM_PROCESSORS   	1u

#define	INITIAL_TID		((tid_t)1u)
#define INIT_SERVER_TID		INITIAL_TID
#define INIT_PAGER_TID		(INITIAL_TID+1)

/* This assumes a uniprocessor system */

/* Touching the data types of this struct may break the context switcher! */
/// Contains the necessary data for a thread

struct ThreadControlBlock
{
  dword rootPageMap;
  unsigned char quantaLeft;
  unsigned char threadState;
  unsigned char priority;
  ExecutionState execState; // 48 bytes

  pid_t exHandler; // Send interrupts and exceptions to this port
  tid_t tid;

  union WaitStruct {
    struct Port *port;   // wait to send to this port
    struct ThreadControlBlock *thread;  // wait to receive from this thread
  } wait;

  queue_t receiverWaitQueue; // queue of threads waiting to receive a message from this thread
  int messageBuffer[13];
};

typedef struct ThreadControlBlock tcb_t;

typedef struct TimerDelta
{
  int delta;
  tcb_t *thread;
} timer_delta_t;

tcb_t *createThread( addr_t threadAddr, paddr_t addrSpace, addr_t uStack, pid_t exHandler );
int releaseThread( tcb_t *thread );

tcb_t *getTcb(tid_t tid);

int sleepThread( tcb_t *thread, int msecs );
int startThread( tcb_t *thread );
int pauseThread( tcb_t *thread );
int sysYield( tcb_t *thread );

extern tcb_t *initServerThread, *initPagerThread;
extern tcb_t *currentThread;
extern tree_t tcbTree;

#endif /* THREAD_H */
