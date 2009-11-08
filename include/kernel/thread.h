#ifndef THREAD_H
#define THREAD_H

#include <types.h>
#include <oslib.h>
#include <kernel/mm.h>

#define DEAD			0
#define PAUSED			1  // Infinite blocking state
#define SLEEPING		2  // Blocks until timer runs out
#define READY			3  // <-- Do not touch(or the context switch code will break)
#define RUNNING			4  // <-- Do not touch(or the context switch code will break)
#define WAIT_FOR_SEND		5  
#define WAIT_FOR_RECV		6
#define ZOMBIE			7

#define NUM_PROCESSORS   1

#define TID_MAX		(tid_t)0x7FFF

#define yieldCPU() asm __volatile__("int    $32\n")

typedef struct RegisterState Registers;

#define	INITIAL_TID	(tid_t)0
#define IDLE_TID	INITIAL_TID

#define GET_TID(t)	(tid_t)(t - tcbTable)

/// Represents a node link in a queue

struct NodePointer
{
  union
  {
    tid_t prev;
    unsigned short delta;
  };
  tid_t next;
};

/// Represents a queue of threads

struct Queue
{
  tid_t head;
  tid_t tail;
};

/* This assumes only *single* processors */

/* Touching the data types of this struct may break the context switcher! */

/// Contains the necessary data for a thread

struct ThreadCtrlBlk
{
  volatile addr_t addrSpace;
  volatile unsigned char quantaLeft;
  volatile unsigned char state : 4;
  volatile unsigned char priority : 3;
  volatile unsigned char resd : 1;
  struct Queue threadQueue;
  tid_t exHandler;
  tid_t wait_tid;
  void *sig_handler;
  byte *io_bitmap;
  unsigned short __packing;
  volatile Registers regs;
} __PACKED__;

typedef struct ThreadCtrlBlk TCB;

tid_t init_server_tid;
TCB *tcbTable;
struct NodePointer *tcbNodes;
struct Queue *runQueues;
//struct Queue pausedQueue;
struct Queue sleepQueue;

int maxThreads;

addr_t createAddrSpace( void );

TCB  volatile * volatile currentThread;

tid_t getFreeTID(void);

TCB *createThread( addr_t threadAddr, addr_t addrSpace, addr_t uStack, tid_t exHandler/*, byte *io_bitmap*/ );
int releaseThread( TCB *thread, tid_t tid );

int sleepThread( TCB *thread, int msecs );
int startThread( TCB *thread );
int pauseThread( TCB *thread );
int sysYield( TCB *thread );

//int switchToThread( volatile TCB *oldThread, volatile TCB *newThread );

#endif /* THREAD_H */
