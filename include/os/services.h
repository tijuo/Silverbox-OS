#ifndef OS_SERVICES_H
#define OS_SERVICES_H

#include <types.h>
#include <os/region.h>
#include <os/vfs.h>

#define SERVER_GENERIC		0
#define SERVER_DEVICE		1

#define MAP_MEM			1
#define UNMAP_MEM		2
#define REGISTER_SERVER		3
#define UNREGISTER_SERVER	4

#define REGISTER_NAME		9
#define LOOKUP_NAME		10
//#define LOOKUP_TID		4
//#define MAP_TID			5
//#define CREATE_SHM		6
//#define ATTACH_SHM_REG		7
//#define DETACH_SHM_REG		8
//#define DELETE_SHM		9
//#define CONNECT_REQ		10
#define UNREGISTER_NAME		11
//#define CHANGE_IO_PERM		12
//#define DEV_REGISTER            13
//#define DEV_LOOKUP_MAJOR        14
//#define DEV_LOOKUP_NAME         15
//#define DEV_UNREGISTER		16
//#define EXEC			17

//#define MAP_REGION		18
//#define UNMAP_REGION		19
#define CREATE_PORT		5
//#define LISTEN_PORT		21
#define DESTROY_PORT		6
#define SEND_MESSAGE		7
#define RECEIVE_MESSAGE		8


#define GEN_REPLY_TYPE		0x80000000
#define SHARE_MEM_REQ		0xFFF0

#define	MEM_FLG_RO		    0x01		// Read only
#define MEM_FLG_EAGER		0x02		// Map in pages immediately (instead of only when they are accessed)
#define MEM_FLG_ALLOC		0x04		// Do not perform a phys->virt mapping (implies read-write)
#define MEM_FLG_COW		    0x08		// Mark as copy-on-write(implies read-only)
#define MEM_FLG_IO		    0x10		// Map IO memory instead
#define MEM_FLG_NOCACHE     0x20

struct GenericReq
{
  int request;
  int arg[8];
};

struct ExecReq
{
  size_t nameLen;
  size_t argsLen;
  char data[];
};

addr_t mapMem(addr_t addr, int device, size_t length, uint64_t offset, int flags);
int unmapMem(addr_t addr, size_t length);
pid_t createPort(pid_t port, int flags);
int destroyPort(pid_t port);
int registerServer(int type, int id);
int unregisterServer(void);
int registerName(const char *name);
tid_t lookupName(const char *name);
int unregisterName(const char *name);
int getCurrentTime(unsigned int *time);

int listDir( const char *path, size_t maxEntries, struct FileAttributes *attrib );
int getFileAttributes( const char *path, struct FileAttributes *attrib );
int readFile( const char *path, int offset, char *buffer, size_t bytes );

int mountFs( int device, const char *fs, size_t fsLen, const char *path, int flags );
int unmountFs( const char *path );

int exec( char *filename, char *args );


// Kernel Services

int irqBind(unsigned int irq);
int irqUnbind(unsigned int irq);
int irqEoi(unsigned int irq);

#endif /* OS_SERVICES_H */
