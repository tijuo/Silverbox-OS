#include <oslib.h>
#include <os/services.h>
#include <os/message.h>
#include <os/region.h>
#include <os/vfs.h>
#include <stdlib.h>
#include <string.h>
#include <os/dev_interface.h>
#include <os/device.h>

// Maps a physical address range to a virtual address range
int mapMem( void *phys, void *virt, int numPages, int flags );

// Reserves a virtual address range
int allocatePages( void *address, int numPages );

// Associates a tid with an address space
int mapTid( tid_t tid, rspid_t pool_id );

// Creates a region of shared memory
int createShmem( shmid_t shmid, unsigned pages, struct MemRegion *region,
                bool ro_perm );

// Attaches a memory region to an existing shared memory region
int attachShmemReg( shmid_t shmid, struct MemRegion *region );

// Associates a name with the current thread ID
int registerName( const char *name, size_t len );

// Returns the TID associated with a name
tid_t lookupName( const char *name, size_t len );

// Associates a name with a device
int registerDevice( const char *name, size_t name_len, struct Device *deviceInfo );

// Returns a device associated with a device major number
int lookupDevMajor( unsigned char major, struct Device *device );


int registerFs( const char *name, size_t name_len, struct Filesystem *fsInfo );


int lookupFsName( const char *name, size_t name_len, struct Filesystem *fs );

/*
int allocatePortRange( int first_port, int num_ports );
int releasePortRange( int first_port, int num_ports );
*/

/*
void handleConnection( struct Message *msg )
{
  struct MessageHeader *header = (struct MessageHeader *)msg->data;
  struct ConnectArgs *args = (struct ConnectArgs *)(header+1);
  size_t pages = (args->region.length % 4096 == 0 ? args->region.length / 4096 : args->region.length / 4096 + 1);
  struct MemRegion region = { 0x800000, pages * 4096 };
  
  createShmem(0, pages, &region, 0);
  attachShmemReg(0, &args->region);
}
*/

int mapMem( void *phys, void *virt, int numPages, int flags )
{
  struct GenericReq *req;
  struct Message msg;
  int status;

  req = (struct GenericReq *)msg.data;

  req->request = MAP_MEM;
  req->arg[0] = (int)phys;
  req->arg[1] = (int)virt;
  req->arg[2] = (int)numPages;
  req->arg[3] = flags;

  msg.length = sizeof *req;
  msg.protocol = MSG_PROTO_GENERIC;

  while( (status=__send( INIT_SERVER, &msg, 0 )) == 2 );

  if( status < 0 )
    return -1;

  while( __receive( INIT_SERVER, &msg, 0 ) == 2 );

  return req->arg[0];
}

int allocatePages( void *address, int numPages )
{
  return mapMem( NULL_PADDR, address, numPages, MEM_FLG_LAZY | MEM_FLG_ALLOC );
}

int mapTid( tid_t tid, rspid_t pool_id )
{
  volatile struct GenericReq *req;
  volatile struct Message msg;

  req = (volatile struct GenericReq *)msg.data;

  req->request = MAP_TID;
  req->arg[0] = (int)tid;
  req->arg[1] = (int)pool_id;

  msg.length = sizeof *req;
  msg.protocol = MSG_PROTO_GENERIC;

  while( __send( INIT_SERVER, (struct Message *)&msg, 0 ) == 2 );
  while( __receive( INIT_SERVER, (struct Message *)&msg, 0 ) == 2 );

  return req->arg[0];
}

int createShmem( shmid_t shmid, unsigned pages, struct MemRegion *region, 
                bool ro_perm )
{
  volatile struct GenericReq *req;
  volatile struct Message msg;

  req = (volatile struct GenericReq *)msg.data;

  if( region == NULL )
    return -1;

  req->request = CREATE_SHM;
  req->arg[0] = (int)shmid;
  req->arg[1] = (int)pages;
  req->arg[2] = (int)ro_perm;
  req->arg[3] = (int)region->start;
  req->arg[4] = (int)region->length;

  msg.length = sizeof *req;
  msg.protocol = MSG_PROTO_GENERIC;

  while( __send( INIT_SERVER, (struct Message *)&msg, 0 ) == 2 );
  while( __receive( INIT_SERVER, (struct Message *)&msg, 0 ) == 2 );

  return req->arg[0];
}

int attachShmemReg( shmid_t shmid, struct MemRegion *region )
{
  volatile struct GenericReq *req;
  volatile struct Message msg;

  req = (volatile struct GenericReq *)msg.data;

  if( region == NULL )
    return -1;

  req->request = ATTACH_SHM_REG;
  req->arg[0] = (int)shmid;
  req->arg[1] = (int)region->start;
  req->arg[2] = (int)region->length;

  msg.length = sizeof *req;
  msg.protocol = MSG_PROTO_GENERIC;

  while( __send( INIT_SERVER, (struct Message *)&msg, 0 ) == 2 );
  while( __receive( INIT_SERVER, (struct Message *)&msg, 0 ) == 2 );

  return req->arg[0];
}

int registerName( const char *name, size_t len )
{
  volatile struct GenericReq *req;
  volatile struct Message msg;

  req = (volatile struct GenericReq *)msg.data;

  if( name == NULL || len > sizeof(req->arg) - 2 * sizeof(int))
    return -1;

  req->request = REGISTER_NAME;
  req->arg[0] = (int)len;
  memcpy((void *)&req->arg[1], name, len);

  msg.length = sizeof *req;
  msg.protocol = MSG_PROTO_GENERIC;

  while( __send( INIT_SERVER, (struct Message *)&msg, 0 ) == 2 );
  while( __receive( INIT_SERVER, (struct Message *)&msg, 0 ) == 2 );

  return req->arg[0];
}

tid_t lookupName( const char *name, size_t len )
{
  volatile struct GenericReq *req;
  volatile struct Message msg;

  req = (volatile struct GenericReq *)msg.data;

  if( name == NULL || len > sizeof(req->arg) - sizeof(int))
    return -1;

  req->request = LOOKUP_NAME;
  req->arg[0] = (int)len;
  memcpy((void *)&req->arg[1], name, len);

  msg.length = sizeof *req;
  msg.protocol = MSG_PROTO_GENERIC;

  while( __send( INIT_SERVER, (struct Message *)&msg, 0 ) == 2 );
  while( __receive( INIT_SERVER, (struct Message *)&msg, 0 ) == 2 );

  return (tid_t)req->arg[0];
}

int registerDevice( const char *name, size_t name_len, struct Device *deviceInfo )
{
  struct RegisterNameReq *req;
  volatile struct DevMgrReply *reply;
  volatile struct Message msg;

  req = (struct RegisterNameReq *)msg.data;
  reply = (volatile struct DevMgrReply *)msg.data;

  if( name == NULL || name_len > N_MAX_NAME_LEN || deviceInfo == NULL)
    return -1;

  req->req_type = DEV_REGISTER;
  req->name_len = name_len;
  strncpy(req->name, name, name_len);
  req->entry.device = *deviceInfo;
  req->name_type = DEV_NAME;

  msg.length = sizeof *req;
  msg.protocol = MSG_PROTO_DEVMGR;

  while( __send( INIT_SERVER, (struct Message *)&msg, 0 ) == 2 );
  while( __receive( INIT_SERVER, (struct Message *)&msg, 0 ) == 2 );
  
  return reply->reply_status;
}

int lookupDevMajor( unsigned char major, struct Device *device )
{
  struct NameLookupReq *req;
  volatile struct DevMgrReply *reply;
  volatile struct Message msg;

  req = (struct NameLookupReq *)msg.data;
  reply = (volatile struct DevMgrReply *)msg.data;

  if( device == NULL )
    return -1;

  req->req_type = DEV_LOOKUP_MAJOR;
  req->major = major;

  msg.length = sizeof *req;
  msg.protocol = MSG_PROTO_DEVMGR;

  while( __send( INIT_SERVER, (struct Message *)&msg, 0 ) == 2 );
  while( __receive( INIT_SERVER, (struct Message *)&msg, 0 ) == 2 );

  if( reply->reply_status == REPLY_SUCCESS )
    *device = reply->entry.device;

  return reply->reply_status;
}

int lookupDevName( const char *name, size_t name_len, struct Device *device )
{
  struct NameLookupReq *req;
  volatile struct DevMgrReply *reply;
  volatile struct Message msg;

  req = (struct NameLookupReq *)msg.data;
  reply = (volatile struct DevMgrReply *)msg.data;

  if( device == NULL || name == NULL || name_len < N_MAX_NAME_LEN )
    return -1;

  req->req_type = DEV_LOOKUP_NAME;
  req->name_len = name_len;   
  strncpy(req->name, name, name_len);

  msg.length = sizeof *req;
  msg.protocol = MSG_PROTO_DEVMGR;

  while( __send( INIT_SERVER, (struct Message *)&msg, 0 ) == 2 );
  while( __receive( INIT_SERVER, (struct Message *)&msg, 0 ) == 2 );

  if( reply->reply_status == REPLY_SUCCESS )
    *device = reply->entry.device;

  return reply->reply_status;
}

int registerFs( const char *name, size_t name_len, struct Filesystem *fsInfo )
{
  struct RegisterNameReq *req;
  volatile struct DevMgrReply *reply;
  volatile struct Message msg;

  req = (struct RegisterNameReq *)msg.data;
  reply = (volatile struct DevMgrReply *)msg.data;

  if( name == NULL || name_len > N_MAX_NAME_LEN || fsInfo == NULL)
    return -1;

  req->req_type = DEV_REGISTER;
  req->name_len = name_len;
  strncpy(req->name, name, name_len);
  req->entry.fs = *fsInfo;
  req->name_type = FS_NAME;

  msg.length = sizeof *req;
  msg.protocol = MSG_PROTO_DEVMGR;

  while( __send( INIT_SERVER, (struct Message *)&msg, 0 ) == 2 );
  while( __receive( INIT_SERVER, (struct Message *)&msg, 0 ) == 2 );
  
  return reply->reply_status;
}

int lookupFsName( const char *name, size_t name_len, struct Filesystem *fs )
{
  struct NameLookupReq *req;
  volatile struct DevMgrReply *reply;
  volatile struct Message msg;

  req = (struct NameLookupReq *)msg.data;
  reply = (volatile struct DevMgrReply *)msg.data;

  if( fs == NULL || name == NULL || name_len < N_MAX_NAME_LEN )
    return -1;

  req->req_type = DEV_LOOKUP_NAME;
  req->name_len = name_len;   
  strncpy(req->name, name, name_len);

  msg.length = sizeof *req;
  msg.protocol = MSG_PROTO_DEVMGR;

  while( __send( INIT_SERVER, (struct Message *)&msg, 0 ) == 2 );
  while( __receive( INIT_SERVER, (struct Message *)&msg, 0 ) == 2 );

  if( reply->reply_status == REPLY_SUCCESS )
    *fs = reply->entry.fs;

  return reply->reply_status;
}

int listDir( const char *path, size_t maxEntries, struct FileAttributes *attrib )
{
  struct FsReqHeader *req;
  volatile struct Message msg;
  struct VfsListArgs *args;
  char *buffer;

  if( !path || !attrib )
    return -1;

  req = (struct FsReqHeader *)msg.data;

  req->request = LIST;
  req->pathLen = strlen(path);
  req->argLen = sizeof(struct VfsListArgs);

  msg.length = sizeof *req;
  msg.protocol = MSG_PROTO_VFS;
 
  while( __send( INIT_SERVER, (struct Message *)&msg, 0 ) == 2 );

  buffer = malloc(sizeof(struct VfsListArgs) + req->pathLen);

  if( !buffer )
    return -1;

  args = (struct VfsListArgs *)(buffer + req->pathLen);

  args->maxEntries = maxEntries;
  memcpy( buffer, path, req->pathLen );

  _send( INIT_SERVER, buffer, sizeof(*args) + req->pathLen, 0 );
  free(buffer);
  while( __receive( INIT_SERVER, (struct Message *)&msg, 0 ) == 2 );

  if( *(int *)msg.data > 0 )
    _receive( INIT_SERVER, attrib, *(int *)msg.data * sizeof(struct FileAttributes), 0 );

  return *(int *)msg.data;
}

int getFileAttributes( const char *path, struct FileAttributes *attrib )
{
  struct FsReqHeader *req;
  volatile struct Message msg;
  struct VfsGetAttribArgs *args;
  char *buffer;

  if( !path || !attrib )
    return -1;

  req = (struct FsReqHeader *)msg.data;

  req->request = GET_ATTRIB;
  req->pathLen = strlen(path);
  req->argLen = sizeof(struct VfsGetAttribArgs);

  msg.length = sizeof *req;
  msg.protocol = MSG_PROTO_VFS;
 
  while( __send( INIT_SERVER, (struct Message *)&msg, 0 ) == 2 );

  buffer = malloc(sizeof(struct VfsGetAttribArgs) + req->pathLen);

  if( !buffer )
    return -1;

  args = (struct VfsGetAttribArgs *)(buffer + req->pathLen);

  memcpy( buffer, path, req->pathLen );

  _send( INIT_SERVER, buffer, sizeof(*args) + req->pathLen, 0 );
  free(buffer);
  while( __receive( INIT_SERVER, (struct Message *)&msg, 0 ) == 2 );

  if( *(int *)msg.data == 0 )
    _receive( INIT_SERVER, attrib, sizeof(struct FileAttributes), 0 );

  return *(int *)msg.data;
}

int readFile( const char *path, int offset, char *buffer, size_t bytes )
{
  struct FsReqHeader *req;
  volatile struct Message msg;
  struct VfsReadArgs *args;
  char *buf;

  if( !path || !buffer )
    return -1;

  req = (struct FsReqHeader *)msg.data;

  req->request = READ;
  req->pathLen = strlen(path);
  req->argLen = sizeof(struct VfsReadArgs);

  msg.length = sizeof *req;
  msg.protocol = MSG_PROTO_VFS;
 
  while( __send( INIT_SERVER, (struct Message *)&msg, 0 ) == 2 );

  buf = malloc(sizeof(struct VfsReadArgs) + req->pathLen);

  if( !buf )
    return -1;

  args = (struct VfsReadArgs *)(buf + req->pathLen);

  args->offset = offset;
  args->length = bytes;

  memcpy( buf, path, req->pathLen );

  _send( INIT_SERVER, buf, sizeof(*args) + req->pathLen, 0 );
  free(buf);
  while( __receive( INIT_SERVER, (struct Message *)&msg, 0 ) == 2 );

  if( *(int *)msg.data > 0 )
    _receive( INIT_SERVER, buffer, *(int *)msg.data, 0 );

  return *(int *)msg.data;
}

int mountFs( int device, const char *fs, size_t fsLen, const char *path, int flags )
{
  struct FsReqHeader *req;
  volatile struct Message msg;
  size_t pathLen;
  struct MountArgs *mountArgs;
  char *buffer;

  if( path == NULL )
    return -1;

  pathLen = strlen(path);

  req = (struct FsReqHeader *)msg.data;

  req->request = MOUNT;
  req->pathLen = pathLen;
  req->argLen = sizeof(struct MountArgs);

  msg.length = sizeof *req;
  msg.protocol = MSG_PROTO_VFS;

  buffer = malloc(sizeof(struct MountArgs) + pathLen);

  if( !buffer )
    return -1;

  while( __send( INIT_SERVER, (struct Message *)&msg, 0 ) == 2 );

  mountArgs = (struct MountArgs *)(buffer + pathLen);

  memcpy( buffer, path, pathLen );

  mountArgs->device = device;
  mountArgs->fsLen = fsLen;
  memcpy(mountArgs->fs, fs, fsLen);
  mountArgs->flags = flags;

  _send( INIT_SERVER, buffer, sizeof(struct MountArgs) + pathLen, 0 );
  while( __receive( INIT_SERVER, (struct Message *)&msg, 0 ) == 2 );
  free(buffer);
  return *(int *)msg.data;
}

int unmountFs( const char *path )
{
  struct FsReqHeader *req;
  volatile struct Message msg;
  size_t pathLen;

  req = (struct FsReqHeader *)msg.data;

  if( path == NULL )
    return -1;

  pathLen = strlen(path);

  req->request = UNMOUNT;
  req->pathLen = pathLen;
  req->argLen = 0;

  msg.length = sizeof *req;
  msg.protocol = MSG_PROTO_VFS;

  while( __send( INIT_SERVER, (struct Message *)&msg, 0 ) == 2 );

  _send( INIT_SERVER, path, pathLen, 0 );
  
  while( __receive( INIT_SERVER, (struct Message *)&msg, 0 ) == 2 );

  return *(int *)msg.data;
}
