#include <oslib.h>

int __send( tid_t recipient, void *msg, int timeout )
{
  int retval;

  asm __volatile__("int %0\n" :: "i"(SYSCALL_INT), "a"(SYS_SEND),
                   "b"(recipient), "c"(msg), "d"(timeout));
  asm __volatile__("mov %%eax, %0\n" : "=m"(retval));
  return retval;
}

int __receive( tid_t sender, void *buf, int timeout )
{
  int retval;

  asm __volatile__("int %0\n" :: "i"(SYSCALL_INT), "a"(SYS_RECEIVE),
                   "b"(sender), "c"(buf), "d"(timeout));
  asm __volatile__("mov %%eax, %0\n" : "=m"(retval));
  return retval;
}

int __pause( void )
{
  int retval;

  asm __volatile__("int %0\n" :: "i"(SYSCALL_INT), "a"(SYS_PAUSE));
  asm __volatile__("mov %%eax, %0\n" : "=m"(retval));
  return retval;
}

int __start_thread( tid_t tid )
{
  int retval;

  asm __volatile__("int %0\n" :: "i"(SYSCALL_INT), "a"(SYS_START_THREAD),
                   "b"(tid));
  asm __volatile__("mov %%eax, %0\n" : "=m"(retval));
  return retval;
}

int __get_thread_info( tid_t tid, struct ThreadInfo *info )
{
  int retval;

  asm __volatile__("int %0\n" :: "i"(SYSCALL_INT), "a"(SYS_GET_THREAD_INFO),
                   "b"(tid), "c"(info));
  asm __volatile__("mov %%eax, %0\n" : "=m"(retval));
  return retval;
}

int __register_int( int intNum )
{
  int retval;

  asm __volatile__("int %0\n" :: "i"(SYSCALL_INT), "a"(SYS_REGISTER_INT),
                   "b"(intNum));
  asm __volatile__("mov %%eax, %0\n" : "=m"(retval));
  return retval;
}

tid_t __create_thread( void *entry, void *addr_space, void *stack, 
                       tid_t exhandler )
{
  int retval;

  asm __volatile__("int %0\n" :: "i"(SYSCALL_INT), "a"(SYS_CREATE_THREAD),
                   "b"(entry), "c"(addr_space), "d"(stack),
                   "S"(exhandler));
  asm __volatile__("mov %%eax, %0\n" : "=m"(retval));
  return retval;
}

int __map( void *virt, void *phys, size_t pages )
{
  int retval;

  asm __volatile__("int %0\n" :: "i"(SYSCALL_INT), "a"(SYS_MAP),
                   "b"(virt), "c"(phys), "d"(pages));
  asm __volatile__("mov %%eax, %0\n" : "=m"(retval));
  return retval;
}

int __map_page_table( void *virt, void *phys )
{
  int retval;

  asm __volatile__("int %0\n" :: "i"(SYSCALL_INT), "a"(SYS_MAP_PAGE_TABLE),
                   "b"(virt), "c"(phys));
  asm __volatile__("mov %%eax, %0\n" : "=m"(retval));
  return retval;
}

int __grant( void *srcAddr, void *destAddr, void *addrSpace, size_t pages )
{
  int retval;

  asm __volatile__("int %0\n" :: "i"(SYSCALL_INT), "a"(SYS_GRANT),
                   "b"(srcAddr), "c"(destAddr), "d"(addrSpace),
                   "S"(pages));
  asm __volatile__("mov %%eax, %0\n" : "=m"(retval));
  return retval;
}

int __grant_page_table( void *srcAddr, void *destAddr, void *addrSpace, 
                        size_t pages )
{
  int retval;

  asm __volatile__("int %0\n" :: "i"(SYSCALL_INT), "a"(SYS_GRANT_PAGE_TABLE),
                   "b"(srcAddr), "c"(destAddr), "d"(addrSpace),
                   "S"(pages));
  asm __volatile__("mov %%eax, %0\n" : "=m"(retval));
  return retval;
}


void __exit( int status )
{
  asm __volatile__("int %0\n" :: "i"(SYSCALL_INT), "a"(SYS_EXIT),
                   "b"(status));
}

void __yield( void )
{
  __sleep( 0 );
}

int __sleep( int msecs )
{
  int retval;

  asm __volatile__("int %0\n" :: "i"(SYSCALL_INT), "a"(SYS_SLEEP),
                   "b"(msecs));
  asm __volatile__("mov %%eax, %0\n" : "=m"(retval));
  return retval;
}

int __end_irq( int irqNum )
{
  int retval;

  asm __volatile__("int %0\n" :: "i"(SYSCALL_INT), "a"(SYS_END_IRQ),
                   "b"(irqNum));
  asm __volatile__("mov %%eax, %0\n" : "=m"(retval));
  return retval;
}

int __end_page_fault( tid_t tid )
{
  int retval;

  asm __volatile__("int %0\n" :: "i"(SYSCALL_INT), "a"(SYS_END_PAGE_FAULT),
                   "b"(tid));
  asm __volatile__("mov %%eax, %0\n" : "=m"(retval));
  return retval;
}

void *__unmap( void *virt )
{
  void *retval;

  asm __volatile__("int %0\n" :: "i"(SYSCALL_INT), "a"(SYS_UNMAP),
                   "b"(virt) );
  asm __volatile__("mov %%eax, %0\n" : "=m"(retval));
  return retval;
}

void *__unmap_page_table( void *virt )
{
  void *retval;

  asm __volatile__("int %0\n" :: "i"(SYSCALL_INT), "a"(SYS_UNMAP_PAGE_TABLE),
                   "b"(virt) );
  asm __volatile__("mov %%eax, %0\n" : "=m"(retval));
  return retval;
}

int __raise( int signal, int arg )
{
  int retval;

  asm __volatile__("int %0\n" :: "i"(SYSCALL_INT), "a"(SYS_RAISE),
                   "b"(signal), "c"(arg) );
  asm __volatile__("mov %%eax, %0\n" : "=m"(retval));
  return retval;
}

int __set_sig_handler( void *handler )
{
  int retval;

  asm __volatile__("int %0\n" :: "i"(SYSCALL_INT), "a"(SYS_SET_SIG_HANDLER),
                   "b"(handler) );
  asm __volatile__("mov %%eax, %0\n" : "=m"(retval));
  return retval;
}

