#include <os/syscalls.h>

int sys_send(tid_t recipient, const int args[5], int block)
{
  int retval;

  __asm__ __volatile__("int %1\n" : "=a"(retval)
                     : "i"(SYSCALL_INT),
                       "a"((block ? SYS_SEND_WAIT : SYS_SEND) | (recipient << 16)),
                       "b"(args[0]), "c"(args[1]), "d"(args[2]),
                       "S"(args[3]), "D"(args[4]));

  return retval;
}

int sys_call(tid_t recipient, const int inArgs[5], int *outArgs, int block)
{
  int retval;
  int args[5];

  __asm__ __volatile__("int %6\n" : "=a"(retval),
                       "=b"(args[0]),
                       "=c"(args[1]), "=d"(args[2]),
                       "=S"(args[3]), "=D"(args[4])
                     : "i"(SYSCALL_INT),
                       "a"((block ? SYS_CALL_WAIT : SYS_CALL) | (recipient << 16)),
                       "b"(inArgs[0]), "c"(inArgs[1]), "d"(inArgs[2]),
                       "S"(inArgs[3]), "D"(inArgs[4]));

  if(outArgs)
    *outArgs = *args;

  return retval;
}

int sys_receive(tid_t sender, int *outArgs, int block)
{
  int retval;
  int args[5];

  __asm__ __volatile__("int %6\n" : "=a"(retval),
                       "=b"(args[0]),
                       "=c"(args[1]), "=d"(args[2]),
                       "=S"(args[3]), "=D"(args[4])
                     : "i"(SYSCALL_INT),
                       "a"((block ? SYS_RECEIVE_WAIT : SYS_RECEIVE) | (sender << 16)));

  if(outArgs)
    *outArgs = *args;

  return retval;
}

void sys_exit(int code)
{
  __asm__ __volatile__("int %0" :: "i"(SYSCALL_INT),
                       "a"(SYS_EXIT), "b"(code));
}

int sys_wait(unsigned int timeout)
{
  int retval;

  __asm__ __volatile__("int %1" : "=a"(retval)
                                : "i"(SYSCALL_INT),
                                  "a"(SYS_WAIT), "b"(timeout));

  return retval;
}

int sys_map(u32 rootPmap, addr_t vaddr, pframe_t pframe, int flags)
{
  int retval;

  __asm__ __volatile__("int %1" : "=a"(retval)
                                : "i"(SYSCALL_INT),
                                  "a"(SYS_MAP), "b"(rootPmap),
                                  "c"(vaddr), "d"(pframe),
                                  "S"(flags));

  return retval;
}

int sys_unmap(u32 rootPmap, addr_t vaddr)
{
  int retval;

  __asm__ __volatile__("int %1" : "=a"(retval)
                                : "i"(SYSCALL_INT),
                                  "a"(SYS_UNMAP), "b"(rootPmap),
                                  "c"(vaddr));

  return retval;
}

int sys_create_thread(addr_t entry, u32 rootPmap, addr_t stackTop)
{
  int retval;

  __asm__ __volatile__("int %1" : "=a"(retval)
                                : "i"(SYSCALL_INT),
                                  "a"(SYS_CREATE_THREAD), "b"(entry),
                                  "c"(rootPmap), "d"(stackTop));

  return retval;
}

int sys_destroy_thread(tid_t tid)
{
  int retval;

  __asm__ __volatile__("int %1" : "=a"(retval)
                                : "i"(SYSCALL_INT),
                                  "a"(SYS_DESTROY_THREAD), "b"(tid));

  return retval;
}

int sys_read_thread(tid_t tid, thread_info_t *info)
{
  int retval;

  __asm__ __volatile__("int %1" : "=a"(retval)
                                : "i"(SYSCALL_INT),
                                  "a"(SYS_READ_THREAD), "b"(tid),
                                  "c"(info));

  return retval;
}

int sys_update_thread(tid_t tid, thread_info_t *info)
{
  int retval;

  __asm__ __volatile__("int %1" : "=a"(retval)
                                : "i"(SYSCALL_INT),
                                  "a"(SYS_UPDATE_THREAD), "b"(tid),
                                  "c"(info));

  return retval;
}

int sys_bind_irq(tid_t tid, int irqNum)
{
  int retval;

  __asm__ __volatile__("int %1" : "=a"(retval)
                                : "i"(SYSCALL_INT),
                                  "a"(SYS_BIND_IRQ), "b"(tid),
                                  "c"(irqNum));

  return retval;
}

int sys_unbind_irq(int irqNum)
{
  int retval;

  __asm__ __volatile__("int %1" : "=a"(retval)
                                : "i"(SYSCALL_INT),
                                  "a"(SYS_UNBIND_IRQ), "b"(irqNum));

  return retval;
}

int sys_eoi(int irqNum)
{
  int retval;

  __asm__ __volatile__("int %1" : "=a"(retval)
                                : "i"(SYSCALL_INT),
                                  "a"(SYS_EOI), "b"(irqNum));

  return retval;
}

int sys_wait_irq(int irqNum)
{
  int retval;

  __asm__ __volatile__("int %1" : "=a"(retval)
                                : "i"(SYSCALL_INT),
                                  "a"(SYS_IRQ_WAIT), "b"(irqNum),
                                  "c"(0));

  return retval;
}

int sys_poll_irq(int irqNum)
{
  int retval;

  __asm__ __volatile__("int %1" : "=a"(retval)
                                : "i"(SYSCALL_INT),
                                  "a"(SYS_IRQ_WAIT), "b"(irqNum),
                                  "c"(1));

  return retval;
}
