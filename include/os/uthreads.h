#ifndef _OS_THREADS_H
#define _OS_THREADS_H

#include <oslib.h>
//#include <os/list.h>

#define MAX_UTHREADS                    32
#define NULL_UTID			65535

#if (MAX_UTHREADS-1) > NULL_UTID
  #error MAX_UTHREADS is too high.
#endif

#define PAGE_SIZE			4096

#define DEAD_STATE			0
#define RUNNING_STATE			1
#define READY_STATE			2
#define PAUSED_STATE			3

#define STACK_TOP                       0xF0000000

#define CALC_STACK_TOP(utid)          (STACK_TOP-utid*PAGE_SIZE)

#define NUM_PRIORITIES          16

typedef	unsigned short utid_t;

struct State
{
   dword edi;
   dword esi;
   dword ebp;
   dword esp;
   dword ebx;
   dword edx;
   dword ecx;
   dword eax;
} __PACKED__;

struct _UThread
{
  utid_t utid;
  tid_t tid;
  struct State state;
  unsigned char status;
  unsigned char priority;
  char *stack;
};

struct _UThread __uthreads[MAX_UTHREADS];
//struct ListType *uthread_lists[NUM_PRIORITIES];
extern void _restore_state( struct State *state ); 
extern void _switch_state( struct State *old_state, struct State *new_state );
void _init_uthreads(void);
utid_t create_uthread( void *start );

#endif
