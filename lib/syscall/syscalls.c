#include <oslib.h>

int sys_send(pid_t out_port, pid_t recipient, const int args[5], int block)
{
  return sys_rpc(out_port, recipient, args, NULL, block);
}

int sys_receive(pid_t in_port, pid_t *sender, int args[5], int block)
{
  int retval;
  unsigned pair;
  pid_t _sender;

  __asm__ __volatile__("int %6\n" : "=a"(args[0]),
                       "=b"(pair),
                       "=c"(args[1]), "=d"(args[2]),
                       "=S"(args[3]), "=D"(args[4])
                     : "i"(SYSCALL_INT),
                       "a"(block ? SYS_RECEIVE_BLOCK : SYS_RECEIVE),
                       "b"((in_port << 16) | (sender ? *sender : NULL_PID)));

  _sender = (pid_t)(pair >> 16);
  retval = args[0] & 0xFFFF;
  args[0] >>= 16;

  if(sender)
    *sender = _sender;

  return (short int)retval;
}

int sys_rpc(pid_t client, pid_t server, const int in_args[5],
            int out_args[5], int block)
{
  int retval;

  if(!in_args || !out_args)
    return ESYS_ARG;

  __asm__ __volatile__("int %5\n" : "=a"(out_args[0]), "=c"(out_args[1]),
                       "=d"(out_args[2]), "=S"(out_args[3]), "=D"(out_args[4])
                     : "i"(SYSCALL_INT),
                       "a"(((in_args[0] & 0xFFFF) << 16)
                           | (block ? SYS_RPC_BLOCK : SYS_RPC)),
                       "b"((client << 16) | server), "c"(in_args[1]),
                       "d"(in_args[2]), "S"(in_args[3]), "D"(in_args[4]));

  retval = out_args[0] & 0xFFFF;
  out_args[0] >>=  16;
  return (short int)retval;
}


void sys_exit(int code)
{
  int args[5] = { SYS_MSG_EXIT, code, 0, 0, 0 };

  sys_rpc(NULL_PID, NULL_PID, args, NULL, 0);
}

int sys_create(int res, void *arg)
{
  int args[5] = { SYS_MSG_CREATE, res, (int)arg, 0, 0 };

  return sys_rpc(NULL_PID, NULL_PID, args, NULL, 0);
}

int sys_read(int res, void *arg)
{
  int args[5] = { SYS_MSG_READ, res, (int)arg, 0, 0 };

  return sys_rpc(NULL_PID, NULL_PID, args, NULL, 0);
}


int sys_update(int res, void *arg)
{
  int args[5] = { SYS_MSG_UPDATE, res, (int)arg, 0, 0 };

  return sys_rpc(NULL_PID, NULL_PID, args, NULL, 0);
}


int sys_destroy(int res, void *arg)
{
  int args[5] = { SYS_MSG_DESTROY, res, (int)arg, 0, 0 };

  return sys_rpc(NULL_PID, NULL_PID, args, NULL, 0);
}

int sys_wait(unsigned int timeout)
{
  int args[5] = { SYS_MSG_WAIT, (int)timeout, 0, 0, 0 };

  return sys_rpc(NULL_PID, NULL_PID, args, NULL, 0);
}
