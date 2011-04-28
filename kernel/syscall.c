#include <kernel/memory.h>
#include <kernel/mm.h>
#include <kernel/debug.h>
#include <kernel/syscall.h>
#include <kernel/schedule.h>
#include <oslib.h>
#include <kernel/error.h>
#include <kernel/message.h>
#include <kernel/signal.h>
#include <os/signal.h>

extern TCB *idleThread;
void _syscall( volatile TCB *_thread, volatile unsigned intInfo );

static int sysGetThreadInfo( TCB *thread, tid_t tid, struct ThreadInfo *info );
static void sysExit( TCB *thread, int code );

extern int sysEndIRQ( TCB *thread, int irqNum );
extern int sysRegisterInt( TCB *thread, int intNum );
extern int sysUnregisterInt( TCB *thread, int intNum );
extern int sysEndPageFault( TCB *thread, tid_t tid );

int privSyscalls[] = { SYS_MAP, SYS_MAP_PAGE_TABLE, SYS_GRANT, SYS_GRANT_PAGE_TABLE,
  SYS_UNMAP, SYS_UNMAP_PAGE_TABLE, SYS_SET_IO_PERM, SYS_REGISTER_INT, SYS_UNREGISTER_INT };

static void sysExit( TCB *thread, int code )
{
  thread->state = ZOMBIE;

  if( thread->exHandler == NULL_TID )
    releaseThread(thread);
  else
    sysRaise(&tcbTable[thread->exHandler], (GET_TID(thread) << 8) | SIGEXIT, code);
}

static int sysGetThreadInfo( TCB *thread, tid_t tid, struct ThreadInfo *info )
{
  TCB *other_thread;
  assert(info != NULL);

  if( info == NULL )
    return -ENULL;

  if( tid >= maxThreads )
    return -1;

  if( tid == NULL_TID )
    other_thread = thread;
  else
    other_thread = &tcbTable[tid];

  if( other_thread != thread && other_thread->exHandler != GET_TID(thread) )
    return -1;

  info->tid = GET_TID(other_thread);
  info->exHandler = other_thread->exHandler;
  info->priority = other_thread->priority;
  info->addr_space = other_thread->addrSpace;
  info->state = other_thread->regs;

  return 0;
}

static int enableIO_Permissions( volatile TCB *thread, unsigned short begin, 
				 unsigned short end, bool value, tid_t tid )
{
  pte_t pte;
  byte *data = (byte *)TEMP_PAGEADDR;
  int result;

  if( tid == NULL_TID )
    tid = GET_TID(thread);

  if( begin < 32768 && end >= 32768 )   // spans two pages
  {
    if( (result=enableIO_Permissions( thread, begin, 32767, value, tid )) != 0 )
      return result;

    return enableIO_Permissions( thread, 32768, end, value, tid );
  }

  if( begin < 32768 && (readPTE( (void *)TSS_IO_PERM_BMP, &pte, tcbTable[tid].addrSpace ) != 0 ||
      !pte.present) )
  {
    return -3;
  }
  else if( begin >= 32768 && (readPTE( (void *)(TSS_IO_PERM_BMP + PAGE_SIZE), &pte, tcbTable[tid].addrSpace ) != 0 ||
      !pte.present) )
  {
    return -3;
  }

  mapTemp((void *)(pte.base << 12));

  if( begin < 32768 ) // spans first page
    ;
  else
  {
    begin -= 32768;
    end -= 32768;
  }

  if( begin / 8 == end / 8 ) // if the range spans only one byte
  {                          // simply modify this byte
    for(unsigned i=begin % 8; i <= end % 8; i++)
    {
      if( value == false )
        data[begin / 8] |= (1 << i);
      else
        data[begin / 8] &= ~(1 << i);
    }
    kprintf("Enable IO? %s. 0x%x 0x%x: 0x%x\n", value ? "yes" : "no", begin, end, data[begin / 8]);
  }
  else // otherwise, modify the beginning and ending byte
  {
    for(unsigned i=begin % 8; i <= 7; i++)
    {
      if( value == false )
        data[begin / 8] |= (1 << i);
      else
        data[begin / 8] &= ~(1 << i);
    }

    for(unsigned i=0; i <= end % 8; i++)
    {
      if( value == false )
        data[end / 8] |= (1 << i);
      else
        data[end / 8] &= ~(1 << i);
    }

    // and set the bytes in between(if any)

    if( end / 8 + 1 >= begin / 8 )
      memset( data + 1, value == false ? 0xFF : 0, (end / 8) - (begin / 8) - 1 );
  }

  unmapTemp();
  return 0;
}

/// Handles a system call

void _syscall( volatile TCB *thread, volatile unsigned intInfo )
{
  TCB *_thread = (TCB *)thread;
  Registers *regs = (Registers *)&_thread->regs;
  int *result = (int *)&regs->eax;

/*
  for( unsigned i=0; i < sizeof privSyscalls / sizeof(int); i++ )
  {
    if( (int)regs->eax == privSyscalls[i] )
    {
      *result = -2;
      break;
    }
  }
*/
  switch ( regs->eax )
  {
    case SYS_SEND:
      *result = sendMessage( _thread, (tid_t)regs->ebx, (void *)regs->ecx, (int)regs->edx );
      break;
    case SYS_RECEIVE:
      *result = receiveMessage( _thread, (tid_t)regs->ebx, (void *)regs->ecx, (int)regs->edx );
      break;
    case SYS_EXIT:
      sysExit( _thread, (int)regs->ebx );
      break;
    case SYS_CREATE_THREAD:
    {
      TCB *new_thread;

      new_thread = createThread( (addr_t)regs->ebx, (regs->ecx == (dword)NULL_PADDR ?
                                 _thread->addrSpace : (addr_t)regs->ecx),
                              (addr_t)regs->edx, (tid_t)regs->esi );
      *result = GET_TID(new_thread);
      break;
    }
    case SYS_PAUSE:
      *result = pauseThread( _thread );
      break;
    case SYS_START_THREAD:
      if( (tid_t)regs->ebx == NULL_TID )
        *result = -1;
      else if( _thread == &tcbTable[(tid_t)regs->ebx] )
      {
        kprintf("Error: Attempted to started self (which has already started!)\n");
        *result = -1;
      }
      else
        *result = startThread( &tcbTable[(tid_t)regs->ebx] );
      break;
    case SYS_SLEEP:
      if( regs->ebx == 0 )
        sysYield( _thread );
      else
        *result = sleepThread( _thread, (int)regs->ebx );
      break;
    case SYS_END_PAGE_FAULT:
      *result = sysEndPageFault( _thread, (tid_t)regs->ebx );
      break;
    case SYS_END_IRQ:
      *result = sysEndIRQ( _thread, (int)regs->ebx );
      break;
    case SYS_REGISTER_INT:
      *result = sysRegisterInt( _thread, (int)regs->ebx );
      break;
    case SYS_MAP:
      *result = sysMap( _thread, (unsigned char *)regs->ebx, (unsigned char *)regs->ecx,
                        (size_t)regs->edx, (int)regs->esi, (addr_t)regs->edi );
      break;
    case SYS_MAP_PAGE_TABLE:
      *result = sysMapPageTable( _thread, (unsigned char *)regs->ebx, (addr_t)regs->ecx, (int)regs->edx, (addr_t)regs->esi );
      break;
    case SYS_GRANT:
      *result = sysGrant( _thread, (unsigned char *)regs->ebx, (unsigned char *)regs->ecx,
                     (addr_t)regs->edx, (size_t)regs->esi );
      break;
    case SYS_GRANT_PAGE_TABLE:
      *result = sysGrantPageTable( _thread, (unsigned char *)regs->ebx, (unsigned char *)regs->ecx,
                      (addr_t)regs->edx, (size_t)regs->esi );
      break;
    case SYS_GET_THREAD_INFO:
      *result = sysGetThreadInfo( _thread, (tid_t)regs->ebx, (struct ThreadInfo *)regs->ecx );
      break;
    case SYS_UNMAP:
      *result = (int)sysUnmap( _thread, (void *)regs->ebx, (addr_t)regs->ecx );
      break;
    case SYS_UNMAP_PAGE_TABLE:
      *result = (int)sysUnmapPageTable( _thread, (void *)regs->ebx, (addr_t)regs->ecx );
      break;
    case SYS_RAISE:
      *result = sysRaise( _thread, (int)regs->ebx, (int)regs->ecx );
      break;
    case SYS_SET_SIG_HANDLER:
      *result = sysSetSigHandler( _thread, (void *)regs->ebx );
      break;
    case SYS_SET_IO_PERM:
      *result = enableIO_Permissions( _thread, (unsigned short)regs->ebx,
				 (unsigned short)regs->ecx, (bool)regs->edx, (tid_t)regs->esi );
      break;
    case SYS_UNREGISTER_INT:
      *result = sysUnregisterInt( _thread, (int)regs->ebx );
      break;
    case SYS_DESTROY_THREAD:
      *result = releaseThread( _thread );
       break;
    default:
      kprintf("Invalid system call: 0x%x %d 0x%x\n", regs->eax,
		GET_TID(_thread), _thread->regs.eip);
      assert(false);
      *result = -1;
      break;
  }

  //kprintf("regs->eax: 0x%x result: 0x%x\n", _thread->regs.eax, *result);
}
