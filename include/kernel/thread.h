#ifndef THREAD_H
#define THREAD_H

#include <types.h>
#include <kernel/mm.h>
#include <kernel/lowlevel.h>
#include <kernel/struct.h>

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

#define	INITIAL_TID		((tid_t)1u)
#define IDLE_TID		INITIAL_TID

#define GET_TID(t)		(t == NULL ? NULL_TID : t->tid)

/* This assumes a uniprocessor system */

/* Touching the data types of this struct may break the context switcher! */
/// Contains the necessary data for a thread

struct ThreadControlBlock
{
  dword cr3;
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

  tid_t tid;
  pid_t waitPort;   // wait to receive from/send to this port
  ExecutionState execState; // 48 bytes
} __PACKED__;

typedef struct ThreadControlBlock TCB;

struct Queue
{
  TCB *head, *tail;
};

TCB *createThread( addr_t threadAddr, paddr_t addrSpace, addr_t uStack, pid_t exHandler );
int releaseThread( TCB *thread );

TCB *getTcb(tid_t tid);

int sleepThread( TCB *thread, int msecs );
int startThread( TCB *thread );
int pauseThread( TCB *thread );
int sysYield( TCB *thread );

extern struct Queue freeThreadQueue;
extern TCB *init_server;
extern TCB *currentThread;
extern TCB *idleThread;
extern tree_t tcbTree;
//extern TCB tcbTable[MAX_THREADS];


//int switchToThread(  TCB *oldThread,  TCB *newThread );

#endif /* THREAD_H */
