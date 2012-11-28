#include <oslib.h>

int sys_send( tid_t recipient, void *msg, int timeout )
{
  int retval;

  asm __volatile__("int %0\n" :: "i"(SYSCALL_INT), "a"(SYS_SEND),
                   "b"(recipient), "c"(msg), "d"(timeout));
  asm __volatile__("mov %%eax, %0\n" : "=m"(retval));
  return retval;
}

int sys_receive( tid_t sender, void *buf, int timeout )
{
  int retval;

  asm __volatile__("int %0\n" :: "i"(SYSCALL_INT), "a"(SYS_RECEIVE),
                   "b"(sender), "c"(buf), "d"(timeout));
  asm __volatile__("mov %%eax, %0\n" : "=m"(retval));
  return retval;
}

int __pause( void )
{
  return sys_pause_thread( NULL_TID );
}

int sys_pause_thread( tid_t tid )
{
  int retval;

  asm __volatile__("int %0\n" :: "i"(SYSCALL_INT), "a"(SYS_PAUSE_THREAD), "b"(tid));
  asm __volatile__("mov %%eax, %0\n" : "=m"(retval));
  return retval;
}

int sys_start_thread( tid_t tid )
{
  int retval;

  asm __volatile__("int %0\n" :: "i"(SYSCALL_INT), "a"(SYS_START_THREAD),
                   "b"(tid));
  asm __volatile__("mov %%eax, %0\n" : "=m"(retval));
  return retval;
}

int sys_destroy_thread( tid_t tid )
{
  int retval;

  asm __volatile__("int %0\n" :: "i"(SYSCALL_INT), "a"(SYS_DESTROY_THREAD),
                   "b"(tid));
  asm __volatile__("mov %%eax, %0\n" : "=m"(retval));
  return retval;
}

int sys_get_thread_info( tid_t tid, struct ThreadInfo *info )
{
  int retval;

  asm __volatile__("int %0\n" :: "i"(SYSCALL_INT), "a"(SYS_GET_THREAD_INFO),
                   "b"(tid), "c"(info));
  asm __volatile__("mov %%eax, %0\n" : "=m"(retval));
  return retval;
}

int sys_register_int( int intNum )
{
  int retval;

  asm __volatile__("int %0\n" :: "i"(SYSCALL_INT), "a"(SYS_REGISTER_INT),
                   "b"(intNum));
  asm __volatile__("mov %%eax, %0\n" : "=m"(retval));
  return retval;
}

int sys_unregister_int( int intNum )
{
  int retval;

  asm __volatile__("int %0\n" :: "i"(SYSCALL_INT), "a"(SYS_UNREGISTER_INT),
                   "b"(intNum));
  asm __volatile__("mov %%eax, %0\n" : "=m"(retval));
  return retval;
}

tid_t sys_create_thread( void *entry, void *addr_space, void *stack,
                       tid_t exhandler )
{
  tid_t retval;

  asm __volatile__("int %0\n" :: "i"(SYSCALL_INT), "a"(SYS_CREATE_THREAD),
                   "b"(entry), "c"(addr_space), "d"(stack),
                   "S"(exhandler));
  asm __volatile__("mov %%eax, %0\n" : "=m"(retval));
  return retval;
}

void sys_exit( int status )
{
  asm __volatile__("int %0\n" :: "i"(SYSCALL_INT), "a"(SYS_EXIT),
                   "b"(status));
}

void __yield( void )
{
  __sleep( 0 );
}

int sys_sleep_thread( int msecs, tid_t tid )
{
  int retval;

  asm __volatile__("int %0\n" :: "i"(SYSCALL_INT), "a"(SYS_SLEEP_THREAD),
                   "b"(msecs));
  asm __volatile__("mov %%eax, %0\n" : "=m"(retval));
  return retval;
}

int __sleep( int msecs )
{
  return sys_sleep_thread( msecs, NULL_TID );
}

int sys_end_irq( int irqNum )
{
  int retval;

  asm __volatile__("int %0\n" :: "i"(SYSCALL_INT), "a"(SYS_END_IRQ),
                   "b"(irqNum));
  asm __volatile__("mov %%eax, %0\n" : "=m"(retval));
  return retval;
}

int sys_end_page_fault( tid_t tid )
{
  int retval;

  asm __volatile__("int %0\n" :: "i"(SYSCALL_INT), "a"(SYS_END_PAGE_FAULT),
                   "b"(tid));
  asm __volatile__("mov %%eax, %0\n" : "=m"(retval));
  return retval;
}

int sys_raise( int signal, int arg )
{
  int retval;

  asm __volatile__("int %0\n" :: "i"(SYSCALL_INT), "a"(SYS_RAISE),
                   "b"(signal), "c"(arg) );
  asm __volatile__("mov %%eax, %0\n" : "=m"(retval));
  return retval;
}

int sys_set_sig_handler( void *handler )
{
  int retval;

  asm __volatile__("int %0\n" :: "i"(SYSCALL_INT), "a"(SYS_SET_SIG_HANDLER),
                   "b"(handler) );
  asm __volatile__("mov %%eax, %0\n" : "=m"(retval));
  return retval;
}

void sys_invalidate_page( addr_t addr )
{
  asm __volatile__("int %0\n" :: "i"(SYSCALL_INT), "a"(SYS_INVALIDATE_PAGE),
                   "b"(addr) );
}

void sys_invalidate_tlb( void )
{
  asm __volatile__("int %0\n" :: "i"(SYSCALL_INT), "a"(SYS_INVALIDATE_TLB));
}

