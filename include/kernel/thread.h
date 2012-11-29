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

#define NULL_TID	 	0
#define TID_MAX		 	0xFFFFu

#define	INITIAL_TID		1u
#define IDLE_TID		INITIAL_TID

#define GET_TID(t)		(tid_t)(t - tcbTable)

#define MAX_THREADS		1024u

/* This assumes a uniprocessor system */

/* Touching the data types of this struct may break the context switcher! */
/// Contains the necessary data for a thread

struct ThreadControlBlock
{
  cr3_t cr3;
  unsigned char quantaLeft;
  unsigned char threadState : 4;
  unsigned char priority : 3;
  unsigned char reschedule : 1;
  unsigned short int privileged : 1;
  unsigned short int pager : 1;
  unsigned short int __packing : 14;
  struct Queue
  {
    struct ThreadControlBlock *head;
    struct ThreadControlBlock *tail;
  } threadQueue;	// the queue of waiting senders
  struct ThreadControlBlock *exHandler;		// Send interrupts and exceptions to this thread
  struct ThreadControlBlock *waitThread;		// wait to receive from/send to this thread
  void *sig_handler;
  struct ThreadControlBlock *queueNext;
  struct ThreadControlBlock *queuePrev;
  struct ThreadControlBlock *timerNext;
  unsigned int timerDelta;
  ExecutionState execState;
  unsigned int __packing2[4];
} __PACKED__;

typedef struct ThreadControlBlock TCB;

struct Queue freeThreadQueue;

TCB *init_server;
TCB *currentThread;
TCB *idleThread;
TCB tcbTable[MAX_THREADS];

TCB *createThread( addr_t threadAddr, addr_t addrSpace, addr_t uStack, TCB *exHandler );
int releaseThread( TCB *thread );

int sleepThread( TCB *thread, unsigned int msecs );
int startThread( TCB *thread );
int pauseThread( TCB *thread );
int sysYield( TCB *thread );

//int switchToThread(  TCB *oldThread,  TCB *newThread );

#endif /* THREAD_H */
