#include <kernel/memory.h>
#include <kernel/mm.h>
#include <kernel/debug.h>
#include <kernel/syscall.h>
#include <kernel/schedule.h>
#include <kernel/error.h>
#include <kernel/message.h>
#include <kernel/signal.h>
#include <os/signal.h>

void _syscall( TCB *thread );

static int sysGetThreadInfo( TCB *thread, tid_t tid, struct ThreadInfo *info );
static void sysExit( TCB *thread, int code );

extern int sysEndIRQ( TCB *thread, int irqNum );
extern int sysRegisterInt( TCB *thread, int intNum );
extern int sysUnregisterInt( TCB *thread, int intNum );
extern int sysEndPageFault( TCB *thread, tid_t tid );

unsigned int privSyscalls[] = { SYS_REGISTER_INT, SYS_UNREGISTER_INT, SYS_SET_THREAD_STATE, SYS_SET_THREAD_PRIORITY, SYS_EOI,
                                SYS_END_PAGE_FAULT };
unsigned int pagerSyscalls[] = { SYS_GET_PAGE_MAPPING, SYS_SET_PAGE_MAPPING, SYS_INVALIDATE_TLB };

static void sysExit( TCB *thread, int code )
{
  thread->threadState = ZOMBIE;

  if( thread->exHandler == NULL_TID )
    releaseThread(thread);
  else
    sysRaise(thread->exHandler, (GET_TID(thread) << 8) | SIGEXIT, code);
}

static int sysGetThreadInfo( TCB *thread, tid_t tid, struct ThreadInfo *info )
{
  TCB *other_thread;
  assert(info != NULL);

  if( info == NULL )
    return -ENULL;

  if( tid >= MAX_THREADS )
    return -1;

  if( tid == NULL_TID )
    other_thread = thread;
  else
    other_thread = &tcbTable[tid];

  if( other_thread != thread && other_thread->exHandler != thread )
    return -1;

  info->tid = GET_TID(other_thread);
  info->exHandler = GET_TID(other_thread->exHandler);
  info->priority = other_thread->priority;
  info->addr_space = (addr_t)(other_thread->cr3.base << 12);
  memcpy(&info->state, &other_thread->execState, sizeof other_thread->execState);

  return 0;
}

/// Handles a system call

void _syscall( TCB *thread )
{
  ExecutionState *execState = (ExecutionState *)&thread->execState;
  int *result = (int *)&execState->user.eax;

  if( !thread->privileged )
  {
    for( unsigned i=0; i < sizeof privSyscalls / sizeof(unsigned); i++ )
    {
      if( (unsigned int)execState->user.eax == privSyscalls[i] )
      {
        *result = -2;
        break;
      }
    }
  }

  if( !thread->pager )
  {
    for( unsigned i=0; i < sizeof pagerSyscalls / sizeof(unsigned); i++ )
    {
      if( (unsigned int)execState->user.eax == pagerSyscalls[i] )
      {
        *result = -2;
        break;
      }
    }
  }

  switch ( execState->user.eax )
  {
    case SYS_SEND:
      *result = sendMessage( thread, (tid_t)execState->user.ebx, (void *)execState->user.ecx, (int)execState->user.edx );
      break;
    case SYS_RECEIVE:
      *result = receiveMessage( thread, (tid_t)execState->user.ebx, (void *)execState->user.ecx, (int)execState->user.edx );
      break;
    // XXX: Is this system call necessary?
    case SYS_EXIT:
      sysExit( thread, (int)execState->user.ebx );
      *result = -1; // sys_exit() should never return
      break;
    case SYS_CREATE_THREAD:
    {
      TCB *new_thread;

      new_thread = createThread( (addr_t)execState->user.ebx, (execState->user.ecx == NULL_PADDR ?
                                 (addr_t)(thread->cr3.base << 12) : (addr_t)execState->user.ecx),
                                 (addr_t)execState->user.edx, ((tid_t)execState->user.esi == NULL_TID ?
                                                               NULL : &tcbTable[(tid_t)execState->user.esi]) );
      *result = GET_TID(new_thread);
      break;
    }
    case SYS_SET_THREAD_STATE:
      *result = -1;
      break;
    case SYS_SET_THREAD_PRIORITY:
      *result = -1;
      break;
    case SYS_SET_PAGE_MAPPING:
      *result = -1;
      break;
    case SYS_GET_PAGE_MAPPING:
      *result = -1;
      break;
    case SYS_SLEEP:
      if( execState->user.ebx == 0 )
        sysYield( thread );
      else
        *result = sleepThread( thread, (int)execState->user.ebx );
      break;
    // XXX: Is this needed?
    case SYS_END_PAGE_FAULT:
      *result = sysEndPageFault( thread, (tid_t)execState->user.ebx );
      break;
    // XXX: Is this needed?
    case SYS_EOI:
      *result = sysEndIRQ( thread, (int)execState->user.ebx );
      break;
    case SYS_REGISTER_INT:
      *result = sysRegisterInt( thread, (int)execState->user.ebx );
      break;
    case SYS_GET_THREAD_INFO:
      *result = sysGetThreadInfo( thread, (tid_t)execState->user.ebx, (struct ThreadInfo *)execState->user.ecx );
      break;
    case SYS_INVALIDATE_TLB:
      *result = -1;
      break;
    case SYS_RAISE:
      *result = sysRaise( thread, (int)execState->user.ebx, (int)execState->user.ecx );
      break;
    case SYS_SET_SIG_HANDLER:
      *result = sysSetSigHandler( thread, (void *)execState->user.ebx );
      break;
    case SYS_UNREGISTER_INT:
      *result = sysUnregisterInt( thread, (int)execState->user.ebx );
      break;
    case SYS_DESTROY_THREAD:
      *result = releaseThread( thread );
       break;
    default:
      kprintf("Invalid system call: 0x%x %d 0x%x\n", execState->user.eax,
		GET_TID(thread), thread->execState.user.eip);
      assert(false);
      *result = -1;
      break;
  }
}
