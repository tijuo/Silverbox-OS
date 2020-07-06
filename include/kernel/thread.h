#ifndef THREAD_H
#define THREAD_H

#include <types.h>
#include <kernel/mm.h>
#include <kernel/lowlevel.h>

#define DEAD			0u
#define PAUSED			1u  // Infinite blocking state
#define SLEEPING		2u  // Blocks until timer runs out
#define READY			3u  // <-- Do not touch(or the context switch code will break)
				   // Thread is ready to be scheduled to a processor
#define RUNNING			4u  // <-- Do not touch(or the context switch code will break)
				   // Thread is already scheduled to a processor
#define WAIT_FOR_SEND		5u
#define WAIT_FOR_RECV		6u
#define ZOMBIE			7u  // Thread is waiting to be released

#define NUM_PROCESSORS   	1u

#define TID_MAX		 	((tid_t)0xFFFFu)

#define	INITIAL_TID		((tid_t)1u)
#define IDLE_TID		INITIAL_TID

#define GET_TID(t)		(t == NULL ? NULL_TID : (tid_t)((t) - tcbTable))

#define MAX_THREADS		1024u

/* This assumes a uniprocessor system */

/* Touching the data types of this struct may break the context switcher! */
/// Contains the necessary data for a thread

struct ThreadControlBlock
{
  cr3_t cr3;
  unsigned char quantaLeft : 7;
  unsigned char kernel : 1;
  unsigned char threadState : 4;
  unsigned char priority : 3;
  unsigned char privileged : 1;
  pid_t exHandler; // Send interrupts and exceptions to this port

  struct
  {
    union
    {
      unsigned int delta;
      tid_t prev;
    };
    tid_t next;
  } queue;

  pid_t waitPort;   // wait to receive from/send to this port
  ExecutionState execState; // 48 bytes
} __PACKED__;

typedef struct ThreadControlBlock TCB;

struct Queue
{
  TCB *head, *tail;
};

TCB *createThread( addr_t threadAddr, addr_t addrSpace, addr_t uStack, pid_t exHandler );
int releaseThread( TCB *thread );

int sleepThread( TCB *thread, int msecs );
int startThread( TCB *thread );
int pauseThread( TCB *thread );
int sysYield( TCB *thread );

#define getTcb(tid)   (((tid_t)(tid) == NULL_TID || (tid_t)(tid) >= MAX_THREADS) ? NULL : (&tcbTable[tid]))

//int switchToThread(  TCB *oldThread,  TCB *newThread );

struct Queue freeThreadQueue;
TCB *init_server;
TCB *currentThread;
TCB *idleThread;
TCB tcbTable[MAX_THREADS];

#endif /* THREAD_H */
