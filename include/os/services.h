#ifndef OS_SERVICES_H
#define OS_SERVICES_H

#include <types.h>
#include <os/device.h>
#include <os/region.h>

#define ALLOC_MEM		1
#define MAP_MEM			2
#define REGISTER_NAME		3
#define LOOKUP_NAME		4
#define LOOKUP_TID		5
#define MAP_TID			6
#define CREATE_SHM		7
#define ATTACH_SHM_REG		8
#define DETACH_SHM_REG		9
#define DELETE_SHM		10
#define CONNECT_REQ		11

#define MSG_REPLY		0x80000000
#define SHARE_MEM_REQ		0xFFF0
#define EXIT_MSG		0xFFFF

/*
DRV_WRITE

1. A wants to write to a device. It sends a DRV_WRITE message to the device server.
2. *The device server sends a CREATE_SHMEM message to the pager.
   The device server also sends an ATTACH_SHMEM_REG on behalf of A.
3. *The pager sends two back an ACKs and/or NACKs to the device server
4. The device server sends an ACK/NACK to A with a window size. The device server waits for a WRITE_DONE message
//A sends an ATTACH_SHMEM_REG message to the pager
//The pager sends back an ACK/NACK to A
5. A writes data to the shared memory region and sends a WRITE_DONE/HANGUP message to the device server
6. The device server reads whatever info from the shared region, does the "write device" operation, and
   sends back a WRITE_RESULT message. 
   *Then it sends a DELETE_SHMEM message to the pager
7. *The pager sends an ACK/NACK back to the device server

DRV_READ

1. A wants to read a device. It sends a DRV_READ message to the device server.
2. *The device server sends a CREATE_SHMEM message to the pager.
   The device server also sends an ATTACH_SHMEM_REG on behalf of A.
3. *The pager sends back an ACK/NACK to the device server.
4. The device server sends an ACK/NACK to A with a window size and result. The device server waits for a READ_DONE message
5. A reads data from the shared memory region and sends a READ_DONE/HANGUP message to the device server
6. *The device server sends a DELETE_SHMEM message to the pager
7. *The pager sends an ACK/NACK back to the device server 
*/

struct GenericReq
{
  int request;
  int arg[8];
} __PACKED__;

struct AllocMemInfo
{
  void *addr;
  unsigned pages;
} __PACKED__;

struct MapMemArgs
{
  void *phys;
  void *virt;
  unsigned pages;
  int flags;
} __PACKED__;

struct MapTidArgs
{
  tid_t tid;
  void *addr;
} __PACKED__;

struct CreateShmArgs
{
  shmid_t shmid;
  unsigned int pages;
  struct MemRegion region;
  bool ro_perm;
} __PACKED__;

struct AttachShmRegArgs
{
  shmid_t shmid;
  struct MemRegion region;
} __PACKED__;

struct DetachShmRegArgs
{
  
} __PACKED__;

struct DeleteShmArgs
{
  
} __PACKED__;

struct ConnectArgs
{
  unsigned int pages;
  struct MemRegion region;
} __PACKED__;

int mapMem( void *phys, void *virt, int numPages, int flags );
int allocatePages( void *address, int numPages );
int mapTid( tid_t tid, void *addr_space );
int createShmem( shmid_t shmid, unsigned pages, struct MemRegion *region, bool ro_perm );
int attachShmemReg( shmid_t shmid, struct MemRegion *region );

int registerDevice( char *name, size_t name_len, struct Device *deviceInfo );
int lookupDevName( char *name, size_t name_len, struct Device *device );
int lookupDevMajor( unsigned char major, struct Device *device );

tid_t lookupName( char *name, size_t len );
int registerName( char *name, size_t len );

int lookupFsName( char *name, size_t name_len, struct Filesystem *fs );
int registerFs( char *name, size_t name_len, struct Filesystem *fsInfo );

#endif /* OS_SERVICES_H */
