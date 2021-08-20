#include <os/syscalls.h>

int sys_send(msg_t *msg)
{
  int retval;

  asm volatile("int %1\n" : "=a"(retval)
                          : "i"(SYSCALL_INT), "a"(SYS_SEND), "b"(msg) : "memory");

  return retval;
}

int sys_call(msg_t *sendMsg, msg_t *recvMsg)
{
  int retval;

  sendMsg->flags |= MSG_CALL;

  asm volatile("int %1\n" : "=a"(retval)
                          : "i"(SYSCALL_INT), "a"(SYS_SEND), "b"(sendMsg), "c"(recvMsg) : "memory");

  return retval;
}

int sys_receive(msg_t *outMsg)
{
  int retval;

  asm volatile("int %1\n" : "=a"(retval)
                          : "i"(SYSCALL_INT), "a"(SYS_RECEIVE), "b"(outMsg) : "memory");

  return retval;
}

void sys_exit(int code)
{
  asm volatile("int %0" :: "i"(SYSCALL_INT),
                       "a"(SYS_EXIT), "b"(code));
}

int sys_wait(int timeout)
{
  int retval;

  asm volatile("int %1" : "=a"(retval)
                                : "i"(SYSCALL_INT),
                                  "a"(SYS_WAIT), "b"(timeout));

  return retval;
}

int sys_map(paddr_t *rootPmap, void *vaddr, pframe_t *pframes, int numPages, unsigned int flags)
{
  int retval;

  asm volatile("int %1" : "=a"(retval)
                                : "i"(SYSCALL_INT),
                                  "a"(SYS_MAP), "b"(rootPmap),
                                  "c"(vaddr), "d"(pframes),
                                  "S"(numPages), "D"(flags) : "memory");

  return retval;
}

int sys_unmap(paddr_t *rootPmap, void *vaddr, int numPages, pframe_t *unmappedFrames)
{
  int retval;

  asm volatile("int %1" : "=a"(retval)
                                : "i"(SYSCALL_INT),
                                  "a"(SYS_UNMAP), "b"(rootPmap),
                                  "c"(vaddr), "d"(numPages),
                                  "S"(unmappedFrames) : "memory");

  return retval;
}

tid_t sys_create_thread(tid_t tid, void *entry, paddr_t *rootPmap, void *stackTop)
{
  int retval;

  asm volatile("int %1" : "=a"(retval)
                                : "i"(SYSCALL_INT),
                                  "a"(SYS_CREATE_THREAD), "b"(tid),
                                  "c"(entry),
                                  "d"(rootPmap), "S"(stackTop) : "memory");

  return (tid_t)retval;
}

int sys_destroy_thread(tid_t tid)
{
  int retval;

  asm volatile("int %1" : "=a"(retval)
                                : "i"(SYSCALL_INT),
                                  "a"(SYS_DESTROY_THREAD), "b"(tid) : "memory");

  return retval;
}

int sys_read_thread(tid_t tid, unsigned int flags, thread_info_t *info)
{
  int retval;

  asm volatile("int %1" : "=a"(retval)
                                : "i"(SYSCALL_INT),
                                  "a"(SYS_READ_THREAD), "b"(tid),
                                  "c"(flags), "d"(info) : "memory");

  return retval;
}

int sys_update_thread(tid_t tid, unsigned int flags, thread_info_t *info)
{
  int retval;

  asm volatile("int %1" : "=a"(retval)
                                : "i"(SYSCALL_INT),
                                  "a"(SYS_UPDATE_THREAD), "b"(tid),
                                  "c"(flags), "d"(info) : "memory");

  return retval;
}

int sys_bind_irq(tid_t tid, unsigned int irqNum)
{
  int retval;

  asm volatile("int %1" : "=a"(retval)
                                : "i"(SYSCALL_INT),
                                  "a"(SYS_BIND_IRQ), "b"(tid),
                                  "c"(irqNum));

  return retval;
}

int sys_unbind_irq(unsigned int irqNum)
{
  int retval;

  asm volatile("int %1" : "=a"(retval)
                                : "i"(SYSCALL_INT),
                                  "a"(SYS_UNBIND_IRQ), "b"(irqNum));

  return retval;
}

int sys_eoi(unsigned int irqNum)
{
  int retval;

  asm volatile("int %1" : "=a"(retval)
                                : "i"(SYSCALL_INT),
                                  "a"(SYS_EOI), "b"(irqNum));

  return retval;
}

int sys_wait_irq(unsigned int irqNum)
{
  int retval;

  asm volatile("int %1" : "=a"(retval)
                                : "i"(SYSCALL_INT),
                                  "a"(SYS_IRQ_WAIT), "b"(irqNum),
                                  "c"(0));

  return retval;
}

int sys_poll_irq(unsigned int irqNum)
{
  int retval;

  asm volatile("int %1" : "=a"(retval)
                                : "i"(SYSCALL_INT),
                                  "a"(SYS_IRQ_WAIT), "b"(irqNum),
                                  "c"(1));

  return retval;
}
