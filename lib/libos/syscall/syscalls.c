#include <os/syscalls.h>

// todo: create a new flag: MSG_SEND to be used internally be the kernel to determine
// todo: whether a message system call is a sysSend/sysCall or sysReceive

int sys_send(msg_t *msg) {
  int retval;

  asm volatile("int %1\n" : "=a"(retval)
      : "i"(SYSCALL_INT), "a"(SYS_SEND), "b"(msg) : "memory");

  return retval;
}

int sys_call(msg_t *sendMsg, msg_t *recvMsg) {
  int retval;

  sendMsg->flags |= MSG_CALL;

  asm volatile("int %1\n" : "=a"(retval)
      : "i"(SYSCALL_INT), "a"(SYS_SEND), "b"(sendMsg), "c"(recvMsg) : "memory");

  return retval;
}

int sys_receive(msg_t *outMsg) {
  int retval;

  asm volatile("int %1\n" : "=a"(retval)
      : "i"(SYSCALL_INT), "a"(SYS_RECEIVE), "b"(outMsg) : "memory");

  return retval;
}

int sys_map(paddr_t *rootPmap, void *vaddr, paddr_t *paddr, int numPages,
            unsigned int flags)
{
  int retval;

  asm volatile("int %1" : "=a"(retval)
      : "i"(SYSCALL_INT),
      "a"(SYS_MAP), "b"(rootPmap),
      "c"(vaddr), "d"(paddr),
      "S"(numPages), "D"(flags) : "memory");

  return retval;
}

int sys_unmap(paddr_t *rootPmap, void *vaddr, int numPages,
              paddr_t *unmappedEntries)
{
  int retval;

  asm volatile("int %1" : "=a"(retval)
      : "i"(SYSCALL_INT),
      "a"(SYS_UNMAP), "b"(rootPmap),
      "c"(vaddr), "d"(numPages),
      "S"(unmappedEntries) : "memory");

  return retval;
}

tid_t sys_create_thread(void *entry, paddr_t *rootPmap, void *stackTop) {
  int retval;

  asm volatile("int %1" : "=a"(retval)
      : "i"(SYSCALL_INT),
      "a"(SYS_CREATE_THREAD),
      "b"(entry),
      "c"(rootPmap), "d"(stackTop) : "memory");

  return (tid_t)retval;
}

int sys_destroy_thread(tid_t tid) {
  int retval;

  asm volatile("int %1" : "=a"(retval)
      : "i"(SYSCALL_INT),
      "a"(SYS_DESTROY_THREAD), "b"(tid) : "memory");

  return retval;
}

int sys_read_thread(tid_t tid, unsigned int flags, thread_info_t *info) {
  int retval;

  asm volatile("int %1" : "=a"(retval)
      : "i"(SYSCALL_INT),
      "a"(SYS_READ_THREAD), "b"(tid),
      "c"(flags), "d"(info) : "memory");

  return retval;
}

int sys_update_thread(tid_t tid, unsigned int flags, thread_info_t *info) {
  int retval;

  asm volatile("int %1" : "=a"(retval)
      : "i"(SYSCALL_INT),
      "a"(SYS_UPDATE_THREAD), "b"(tid),
      "c"(flags), "d"(info) : "memory");

  return retval;
}

int sys_poll(int mask, int isBlocking) {
  asm volatile("mov %%esp, %%edx\n"
      "sysenter\n" :: "i"(SYSCALL_POLL), "b"(mask), "c"(ret));
}
