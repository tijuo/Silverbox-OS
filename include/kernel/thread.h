#ifndef KERNEL_THREAD_H
#define KERNEL_THREAD_H

#include <types.h>
#include <kernel/list_struct.h>
#include <kernel/mm.h>
#include <kernel/lowlevel.h>
#include <os/msg/message.h>

#define MAX_THREADS			65536

#define INACTIVE		    	0
#define PAUSED			    	1  // Infinite blocking state
#define SLEEPING		    	2  // Blocks until timer runs out
#define READY			    	3  // Thread is ready to be scheduled to a processor
#define RUNNING			    	4  // Thread is already scheduled to a processor
#define WAIT_FOR_SEND			5
#define WAIT_FOR_RECV			6
#define ZOMBIE			    	7  // Thread is waiting to be released
#define IRQ_WAIT		    	8

#define NUM_PROCESSORS   		1u

#define GET_TID_START			1024u

#define KERNEL_STACK_SIZE		2048

#define getTid(tcb)			({ __typeof__ (tcb) _tcb=(tcb); (_tcb ? (_tcb - tcbTable) : NULL_TID); })
#define getTcb(tid)			({ __typeof__ (tid) _tid=(tid); (_tid == NULL_TID ? NULL : &tcbTable[_tid]); })

/* This assumes a uniprocessor system */

/* Touching the data types of this struct may break the context switcher! */
/// Contains the necessary data for a thread

struct ThreadControlBlock
{
  dword rootPageMap;
  unsigned char quantaLeft : 7;
  unsigned char waitForKernelMsg : 1;
  unsigned char threadState : 4;
  unsigned char priority : 4;
  tid_t waitTid;
  ExecutionState execState; // 48 bytes
  list_t receiverWaitQueue; // queue of threads waiting to receive a message from this thread
  list_t senderWaitQueue;   // queue of threads waiting to send a message to this thread
  addr_t *extExecState;      // Pointer to 512 byte xsave registers
  addr_t *kernelStack;
  msg_t *pendingMessage;
  msg_t *responseMessage;
  byte _resd[48];
} __PACKED__;

typedef struct ThreadControlBlock tcb_t;

tcb_t *createThread( tid_t desiredTid, addr_t threadAddr, paddr_t addrSpace, addr_t uStack );
int releaseThread( tcb_t *thread );

int sleepThread( tcb_t *thread, int msecs );
int startThread( tcb_t *thread );
int pauseThread( tcb_t *thread );
int sysYield( tcb_t *thread );
addr_t allocateKernelStack(void);
void releaseKernelStack(void *stack);
int saveAndSwitchContext(tcb_t *newThread);
int _saveAndSwitchContext(tcb_t *oldThread, tcb_t *newThread);
void switchContext(tcb_t *newThread);

extern tcb_t *initServerThread;
extern tcb_t *initPagerThread;
extern tcb_t *currentThread;
extern tcb_t *tcbTable;

#endif /* KERNEL_THREAD_H */
