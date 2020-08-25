#include <os/syscalls.h>

int sys_send(const msg_t *msg, int block)
{
  int retval;

  __asm__ __volatile__("int %1\n" : "=a"(retval)
                     : "i"(SYSCALL_INT),
                       "a"((block ? SYS_SEND_WAIT : SYS_SEND) | ((uint32_t)msg->subject << 8) | ((uint32_t)msg->recipient << 16)),
                       "b"(msg->data.u32[0]), "c"(msg->data.u32[1]), "d"(msg->data.u32[2]),
                       "S"(msg->data.u32[3]), "D"(msg->data.u32[4]));

  return (retval & 0xFF) == ESYS_OK ? ESYS_OK : retval;
}

int sys_call(const msg_t *inMsg, msg_t *outMsg, int block)
{
  int retval;

  if(outMsg)
  {
    __asm__ __volatile__("int %6\n" : "=a"(retval),
                         "=b"(outMsg->data.u32[0]),
                         "=c"(outMsg->data.u32[1]), "=d"(outMsg->data.u32[2]),
                         "=S"(outMsg->data.u32[3]), "=D"(outMsg->data.u32[4])
                       : "i"(SYSCALL_INT),
                         "a"((block ? SYS_CALL_WAIT : SYS_CALL) | ((uint32_t)inMsg->subject << 8) | ((uint32_t)inMsg->recipient << 16)),
                         "b"(inMsg->data.u32[0]), "c"(inMsg->data.u32[1]), "d"(inMsg->data.u32[2]),
                         "S"(inMsg->data.u32[3]), "D"(inMsg->data.u32[4]));

    if((retval & 0xFF) == ESYS_OK)
    {
      outMsg->subject = (unsigned char)((retval >> 8) & 0xFF);
      outMsg->sender  = (tid_t)((retval >> 16) & 0xFFFF);
    }
  }
  else
    __asm__ __volatile__("int %1\n" : "=a"(retval)
                       : "i"(SYSCALL_INT),
                         "a"((block ? SYS_CALL_WAIT : SYS_CALL) | ((uint32_t)inMsg->subject << 8) | ((uint32_t)inMsg->recipient << 16)),
                         "b"(inMsg->data.u32[0]), "c"(inMsg->data.u32[1]), "d"(inMsg->data.u32[2]),
                         "S"(inMsg->data.u32[3]), "D"(inMsg->data.u32[4]));

  return ((retval & 0xFF) == ESYS_OK ? ESYS_OK : retval);
}

int sys_receive(msg_t *outMsg, int block)
{
  int retval;

  if(outMsg)
  {
    __asm__ __volatile__("int %6\n" : "=a"(retval),
                       "=b"(outMsg->data.u32[0]),
                       "=c"(outMsg->data.u32[1]), "=d"(outMsg->data.u32[2]),
                       "=S"(outMsg->data.u32[3]), "=D"(outMsg->data.u32[4])
                     : "i"(SYSCALL_INT),
                       "a"((block ? SYS_RECEIVE_WAIT : SYS_RECEIVE) | ((uint32_t)outMsg->recipient << 16)));

    if((retval & 0xFF) == ESYS_OK)
    {
      outMsg->sender = (tid_t)((retval >> 16) & 0xFFFF);
      outMsg->subject = (unsigned char)((retval >> 8) & 0xFF);
    }
  }
  else
    __asm__ __volatile__("int %1\n" : "=a"(retval)
                         : "i"(SYSCALL_INT),
                           "a"((block ? SYS_RECEIVE_WAIT : SYS_RECEIVE) | ((uint32_t)ANY_SENDER << 16)));

  return ((retval & 0xFF) == ESYS_OK ? ESYS_OK : retval);
}

void sys_exit(int code)
{
  __asm__ __volatile__("int %0" :: "i"(SYSCALL_INT),
                       "a"(SYS_EXIT), "b"(code));
}

int sys_wait(int timeout)
{
  int retval;

  __asm__ __volatile__("int %1" : "=a"(retval)
                                : "i"(SYSCALL_INT),
                                  "a"(SYS_WAIT), "b"(timeout));

  return retval;
}

int sys_map(u32 rootPmap, addr_t vaddr, pframe_t pframe, size_t numPages, int flags)
{
  int retval;

  __asm__ __volatile__("int %1" : "=a"(retval)
                                : "i"(SYSCALL_INT),
                                  "a"(SYS_MAP), "b"(rootPmap),
                                  "c"(vaddr), "d"(pframe),
                                  "S"(numPages), "D"(flags));

  return retval;
}

int sys_unmap(u32 rootPmap, addr_t vaddr, size_t numPages)
{
  int retval;

  __asm__ __volatile__("int %1" : "=a"(retval)
                                : "i"(SYSCALL_INT),
                                  "a"(SYS_UNMAP), "b"(rootPmap),
                                  "c"(vaddr), "d"(numPages));

  return retval;
}

tid_t sys_create_thread(tid_t tid, addr_t entry, u32 rootPmap, addr_t stackTop)
{
  int retval;

  __asm__ __volatile__("int %1" : "=a"(retval)
                                : "i"(SYSCALL_INT),
                                  "a"(SYS_CREATE_THREAD), "b"(tid),
                                  "c"(entry),
                                  "d"(rootPmap), "S"(stackTop));

  return (tid_t)retval;
}

int sys_destroy_thread(tid_t tid)
{
  int retval;

  __asm__ __volatile__("int %1" : "=a"(retval)
                                : "i"(SYSCALL_INT),
                                  "a"(SYS_DESTROY_THREAD), "b"(tid));

  return retval;
}

int sys_read_thread(tid_t tid, int flags, thread_info_t *info)
{
  int retval;

  __asm__ __volatile__("int %1" : "=a"(retval)
                                : "i"(SYSCALL_INT),
                                  "a"(SYS_READ_THREAD), "b"(tid),
                                  "c"(flags), "d"(info));

  return retval;
}

int sys_update_thread(tid_t tid, int flags, thread_info_t *info)
{
  int retval;

  __asm__ __volatile__("int %1" : "=a"(retval)
                                : "i"(SYSCALL_INT),
                                  "a"(SYS_UPDATE_THREAD), "b"(tid),
                                  "c"(flags), "d"(info));

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
