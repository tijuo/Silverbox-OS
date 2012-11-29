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

#define PAGE_FAULT_MSG      	0x0E
#define DIE_MSG		    	0xF1FE
#define ALIVE_MSG	    	0xA1B2

#define NULL_PADDR          	0xFFFFFFFFu

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
#define NULL_TID		0

typedef unsigned long shmid_t;
typedef unsigned long rspid_t;

#define MAX_MSG_LEN	MSG_LEN
#define MSG_LEN         1024

#define RAW_PROTOCOL		0

#define PM_PRESENT		0x01
#define PM_NOT_PRESENT		0
#define PM_READ_ONLY		0
#define PM_READ_WRITE		0x02
#define PM_NOT_CACHED		0x10
#define PM_WRITE_THROUGH	0x08
#define PM_NOT_ACCESSED		0
#define PM_ACCESSED		0x20
#define PM_NOT_DIRTY		0
#define PM_DIRTY		0x40
#define PM_LARGE_PAGE		0x80
#define PM_INVALIDATE		0x80000000

struct PageMapping
{
  addr_t virt;
  int level;
  addr_t frame;
  unsigned int flags;
  int status;
};

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
  addr_t virt, phys;
  size_t size;
};

struct ExceptionInfo
{
  tid_t tid;
  unsigned int_num, error_code;
  struct RegisterState state;
  dword cr0, cr2, cr3, cr4;
};

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
