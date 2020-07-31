#ifndef OSLIB_H
#define OSLIB_H

#ifdef __cplusplus
extern "C" {
#endif

#include <types.h>
#include <ipc.h>
#include <os/mutex.h>
#include <os/syscalls.h>

#define INIT_EXHANDLER 		1
#define INIT_SERVER		1
#define	SYSCALL_INT		0x40

#define EXCEPTION_MSG		0xFF00
#define IRQ_MSG			0xFF01

#define PAGE_FAULT_MSG      	0x0E
#define DIE_MSG		    	0xF1FE
#define ALIVE_MSG	    	0xA1B2

#define NULL_PADDR          	((paddr_t)0xFFFFFFFFFFFFFFFFull)

#define IRQ_ANY			-1

#define KERNEL_MBOX_ID 		0

#define REPLY_SUCCESS		0
#define REPLY_FAIL		-2
#define REPLY_ERROR		-1

#define MFL_REQUEST		0x00
#define MFL_REPLY		0x80
#define MFL_MORE		0x40
#define MFL_NOMORE		0x00
#define MFL_ALIVE		0x20
#define MFL_FAIL		0x01
#define MFL_RESEND		0x02
#define MFL_OK			0x00

#define MSG_DATA_LEN		16

#define REQUEST_ALIVE		1  /* Are you alive? */
#define REQUEST_MORE		2  /* More to send */

#define KERNEL_TID		(tid_t)0
#define NULL_RSPID		(rspid_t)0
#define NULL_SHMID		(shmid_t)-1

#define TIMEOUT_INF		((1u << 24) - 1)

typedef unsigned long shmid_t;
typedef unsigned long rspid_t;

typedef unsigned int	pframe_t;

#define MAX_MSG_LEN	MSG_LEN
#define MSG_LEN         1024

#define RAW_PROTOCOL		0

#define TCB_STATUS_DEAD                    0u
#define TCB_STATUS_PAUSED                  1u  // Infinite blocking state
#define TCB_STATUS_SLEEPING                2u  // Blocks until timer runs out
#define TCB_STATUS_READY                   3u  // <-- Do not touch(or the context switc$
                                   // Thread is ready to be scheduled to a p$
#define TCB_STATUS_RUNNING                 4u  // <-- Do not touch(or the context switc$
                                   // Thread is already scheduled to a proce$
#define TCB_STATUS_WAIT_SEND           5u
#define TCB_STATUS_WAIT_RECV           6u
#define TCB_STATUS_ZOMBIE                  7u  // Thread is waiting to be released

struct MemInfo
{
  addr_t virtCodeArea, physCodeArea;
  size_t codeAreaSize;

  addr_t virtDataArea, physDataArea;
  size_t dataAreaSize;

  addr_t bssArea, bssPhysArea;
  size_t bssAreaSize;

  addr_t stackArea, stackPhysArea;
  size_t stackSize;
};

struct PageMapRecord
{
  addr_t virt;
  paddr_t phys;
  size_t size;
};

struct ExceptionInfo
{
  tid_t tid;
  unsigned int_num, error_code;
  struct RegisterState state;
  dword cr0, cr2, cr3, cr4;
};

struct Tcb
{
  int priority;
  int status;
  paddr_t addrSpace;
  pid_t exHandler;

  struct State
  {
    dword eax, ebx, ecx, edx, ebp, esp, edi, esi;
    dword eip, eflags;
  } state;
};

typedef struct Tcb thread_info_t;

struct ExitMsg
{
  tid_t tid;
  int code;
};

typedef struct
{
  long arg[10];
} SyscallArgs;

//#define SAVE_REGS() asm __volatile__("push %ebx\n" "push %esi\n" "push %edi\n")

//#define RESTORE_REGS() asm __volatile__("pop %edi\n" "pop %esi\n" "pop %ebx\n")

//void _sysenter( int callNum, struct SysCallArgs *args );

char *strdup(const char *str);
char *strndup(const char *str, size_t n);
char *strappend(const char *str, const char *add);
char *toHexString( unsigned int num );
char *toIntString( int num );
//char *toOctalString( unsigned int num );
/*
int fatGetAttributes( const char *path, unsigned short devNum, struct FileAttributes *attrib );
int fatGetDirList( const char *path, unsigned short devNum, struct FileAttributes *attrib,
                   size_t maxEntries );
int fatReadFile( const char *path, unsigned int offset, unsigned short devNum, char *fileBuffer, size_t length );
int fatCreateDir( const char *path, const char *name, unsigned short devNum );
int fatCreateFile( const char *path, const char *name, unsigned short devNum );
*/
#ifdef __cplusplus
}
#endif /*__cplusplus */
#endif /* OSLIB_H */
