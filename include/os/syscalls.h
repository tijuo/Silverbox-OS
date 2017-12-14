#ifndef OS_SYSCALLS
#define OS_SYSCALLS

#include <types.h>

#define SYSCALL_INT             0x40

#define SYS_SEND                0x00
#define SYS_WAIT                0x01
//#define SYS_RECEIVE             0x01
#define SYS_EXIT                0x02
#define SYS_ACQUIRE             0x03
#define SYS_RELEASE             0x04
#define SYS_READ                0x05
#define SYS_MODIFY              0x06
#define SYS_GRANT               0x07
#define SYS_REVOKE              0x08

/*
#define SYS_GET_THREAD_INFO     0x03
#define SYS_CREATE_THREAD       0x04
#define SYS_SET_THREAD_STATE    0x05
#define SYS_SET_THREAD_PRIORITY	0x06
#define SYS_SLEEP		0x07
#define SYS_REGISTER_INT        0x08
#define SYS_EOI             	0x09
#define SYS_END_PAGE_FAULT      0x0A
#define SYS_RAISE               0x0B
#define SYS_SET_SIG_HANDLER     0x0C
#define SYS_DESTROY_THREAD      0x0D
#define SYS_UNREGISTER_INT      0x0E
#define SYS_INVALIDATE_TLB	0x0F
#define SYS_SET_PAGE_MAPPING	0x10
#define SYS_GET_PAGE_MAPPING	0x11
#define SYS_GRANT_PRIVILEGE	0x12
*/

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

#define ESYS_OK			0
#define ESYS_ARG		-1
#define ESYS_FAIL		-2
#define ESYS_PERM		-3
#define ESYS_BADCALL		-4
#define ESYS_NOTIMPL		-5

struct PageMapping
{
  addr_t virt;
  int level;
  addr_t frame;
  unsigned int flags;
  int status;
};


struct RegisterState
{
  dword edi;
  dword esi;
  dword ebp;
  dword esp;
  dword ebx;
  dword edx;
  dword ecx;
  dword eax;
  word es;
  word ds;
  dword int_num;
  dword error_code;
  dword eip;
  dword cs;
  dword eflags;
  dword userEsp;
  dword userSs;
};

struct ThreadInfo
{
  tid_t tid;
  tid_t exHandler;
  int priority;
  addr_t addr_space;
  struct RegisterState state;
};

int sys_send( tid_t recipient, void *msg, int timeout );
int sys_receive( tid_t sender, void *buf, int timeout );
int __pause( void );
int sys_set_thread_state( unsigned int state );
int sys_set_thread_priority( unsigned int priority );

//int sys_pause_thread( tid_t tid );
//int sys_start_thread( tid_t tid );
void __yield( void );
int sys_get_thread_info( tid_t tid, struct ThreadInfo *info );
int sys_register_int( int int_num );
int sys_unregister_int( int int_num );
tid_t sys_create_thread( void *entry, void *addr_space,
                       void *user_stack, tid_t exhandler );
int sys_set_page_mapping( struct PageMapping *mappings, size_t len, tid_t tid);
int sys_get_page_mapping( struct PageMapping *mappings, size_t len, tid_t tid);
void sys_exit( int status );
int __sleep( int msecs );
int sys_sleep( int msecs, tid_t tid );
int sys_eoi( int irqNum );
int sys_end_page_fault( tid_t tid );
int sys_raise( int signal, int arg );
int sys_set_sig_handler( void *handler );
int sys_destroy_thread( tid_t tid );
int sys_invalidate_tlb( void );
int sys_grant_privilege( int privilege, tid_t tid );

#endif /* OS_SYSCALLS */
