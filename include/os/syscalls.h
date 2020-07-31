#ifndef OS_SYSCALLS
#define OS_SYSCALLS

#include <types.h>

#define SYSCALL_INT             0x40

#define SYS_EXIT		0
#define SYS_WAIT		1
#define SYS_SEND		2
#define SYS_RECEIVE		3
#define SYS_MAP			4
#define SYS_UNMAP		5
#define SYS_CREATE_THREAD	6
#define SYS_DESTROY_THREAD	7
#define SYS_READ_THREAD		8
#define SYS_UPDATE_THREAD	9
#define SYS_CREATE_PORT		10
#define SYS_DESTROY_PORT	11
#define SYS_EOI			12
#define SYS_IRQ_WAIT		13
#define SYS_BIND_IRQ		14
#define SYS_UNBIND_IRQ		15

#define RES_MAPPING     0
#define RES_IHANDLER    1
#define RES_TCB         2
#define RES_PORT	      3

struct SyscallCreateTcbArgs
{
  addr_t entry;
  paddr_t addrSpace;
  addr_t stack;
  pid_t exHandler;
};

struct SyscallCreateIHandlerArgs
{
  pid_t handler;
  int intNum;
};

struct SyscallCreateMappingArgs
{
  paddr_t addrSpace;
  int entry;
  void *buffer;
  int level;
};

struct SyscallCreatePortArgs
{
  pid_t port;
};

struct SyscallReadMappingArgs
{
  paddr_t addrSpace;
  int entry;
  void *buffer;
  int level;
};

struct SyscallReadTcbArgs
{
  tid_t tid;
  struct Tcb *tcb;
};

struct SyscallUpdateTcbArgs
{
  tid_t tid;
  struct Tcb *tcb;
};

struct SyscallUpdateMappingArgs
{
  paddr_t addrSpace;
  int entry;
  void *buffer;
  int level;
};

struct SyscallDestroyMappingArgs
{
  paddr_t addrSpace;
  int entry;
  int level;
};

struct SyscallDestroyPortArgs
{
  pid_t port;
};

struct SyscallDestroyIHandlerArgs
{
  int intNum;
};

struct SyscallDestroyTcbArgs
{
  tid_t tid;
};

#define PM_PRESENT              0x01
#define PM_NOT_PRESENT          0
#define PM_READ_ONLY            0
#define PM_READ_WRITE           0x02
#define PM_NOT_CACHED           0x10
#define PM_WRITE_THROUGH        0x08
#define PM_NOT_ACCESSED         0
#define PM_ACCESSED             0x20
#define PM_NOT_DIRTY            0
#define PM_DIRTY                0x40
#define PM_LARGE_PAGE           0x80
#define PM_INVALIDATE           0x80000000

#define PRIV_SUPER		0
#define PRIV_PAGER		1

#define ESYS_OK			     0
#define ESYS_ARG		    -1
#define ESYS_FAIL		    -2
#define ESYS_PERM		    -3
#define ESYS_BADCALL		-4
#define ESYS_NOTIMPL		-5
#define ESYS_NOTREADY   -6

struct PageMapping
{
  addr_t virt;
  int level;
  paddr_t frame;
  unsigned int flags;
  int status;
};

struct RegisterState
{
  dword edi;
  dword esi;
  dword ebp;
  dword ebx;
  dword edx;
  dword ecx;
  dword eax;

  dword eip;
  dword cs;
  dword eflags;
  dword userEsp;
  dword userSs;
};

struct ThreadInfo
{
  tid_t tid;
  pid_t exHandler;
  int priority;
  paddr_t addr_space;
  struct RegisterState state;
};

void sys_exit(int code);
int sys_send(pid_t out_port, pid_t recipient, const int args[5], int block);
int sys_receive(pid_t in_port, pid_t *sender, int args[5], int block);
int sys_create(int resource, void *arg);
int sys_read(int resource, void *arg);
int sys_update(int resource, void *arg);
int sys_destroy(int resource, void *arg);
int sys_rpc(pid_t client, pid_t server, const int in_args[5],
            int out_args[5], int block);
int sys_wait(unsigned int timeout);

#endif /* OS_SYSCALLS */
