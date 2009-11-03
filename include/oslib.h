#ifndef OSLIB_H
#define OSLIB_H

#ifdef __cplusplus
extern "C" {
#endif

#include <types.h>
#include <ipc.h>
#include <os/mutex.h>

#define INIT_EXHANDLER 		1
#define INIT_SERVER		1
#define	SYSCALL_INT		0x40

#define SYS_SEND            	0x00
#define SYS_RECEIVE		0x01
#define SYS_EXIT		0x02
#define SYS_MAP			0x03
#define SYS_GRANT		0x04
#define SYS_GET_THREAD_INFO	0x05
#define SYS_CREATE_THREAD	0x06
#define SYS_START_THREAD	0x07
#define SYS_PAUSE		0x08
#define SYS_SLEEP		0x09
#define SYS_REGISTER_INT	0x0A
#define SYS_END_IRQ		0x0B
#define SYS_END_PAGE_FAULT	0x0C
#define SYS_MAP_PAGE_TABLE	0x0D
#define SYS_GRANT_PAGE_TABLE	0x0E
#define SYS_UNMAP		0x0F
#define SYS_UNMAP_PAGE_TABLE	0x10
#define SYS_RAISE		0x11
#define SYS_SET_SIG_HANDLER	0x12
#define SYS_SET_IO_PERM		0x13

#define PAGE_FAULT_MSG      	0x0E
#define DIE_MSG		    	0xF1FE
#define ALIVE_MSG	    	0xA1B2

#define NULL_PADDR          	(void *)0xFFFFFFFF

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
#define NULL_TID		(tid_t)-1
#define NULL_SHMID		(shmid_t)-1  

typedef unsigned long shmid_t;

#define MAX_MSG_LEN	MSG_LEN
#define MSG_LEN         1024

#define RAW_PROTOCOL		0
#define TMPO_PROTOCOL		1
#define UMPO_PROTOCOL		2
#define RMPO_PROTOCOL		3

struct UMPO_Header
{
  unsigned short frag_id : 7;
/*
  union FlagInfo
  {
    unsigned short flags : 3;

    struct FragFlags
    {
      unsigned short more_frags : 1;
      unsigned short retrans : 1;
      unsigned short _resd : 1;
    };
  };
*/
  unsigned short resd : 6;
  unsigned short subject;
} __PACKED__;

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

struct ExceptionInfo
{
  tid_t tid;
  unsigned int_num, error_code;
  struct RegisterState state;
  dword cr0, cr2, cr3, cr4;
};

struct ThreadInfo
{
  tid_t tid;
  tid_t exHandler;
  int priority;
  addr_t addr_space;
  struct RegisterState state;
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

struct LongMsg
{
  size_t length;
  char reply : 1;
  char fail : 1;
  char _resd : 6;
} __PACKED__;

int _receive( tid_t tid, void *buffer, size_t maxLen, int timeout );
int _send( tid_t tid, void *data, size_t len, int timeout );

int __send( tid_t recipient, void *msg, int timeout );
int __receive( tid_t sender, void *buf, int timeout );
int __pause( void );
int __start_thread( tid_t tid );
void __yield( void );
int __get_thread_info( tid_t tid, struct ThreadInfo *info );
int __register_int( int int_num );
tid_t __create_thread( void *entry, void *addr_space, 
                       void *user_stack, tid_t exhandler );
int __map( void *virt, void *phys, size_t pages );
int __map_page_table( void *virt, void *phys );
int __grant( void *src_addr, void *dest_addr, void *addr_space, size_t pages );
int __grant_page_table( void *src_addr, void *dest_addr, void *addr_space, size_t pages );
void __exit( int status );
int __sleep( int msecs );
int __end_irq( int irqNum );
int __end_page_fault( tid_t tid );
void *__unmap( void *virt );
void *__unmap_page_table( void *virt );
int __raise( int signal, int arg );
int __set_sig_handler( void *handler );
int __set_io_perm( unsigned short start, unsigned short end, bool value, tid_t tid );

char *toHexString( unsigned int num );
char *toIntString( int num );
char *toOctalString( unsigned int num );
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
