#include <oslib.h>
#include <os/services.h>
#include <os/message.h>
#include <os/region.h>
#include <os/vfs.h>
#include <stdlib.h>
#include <string.h>
#include <os/dev_interface.h>
#include <os/device.h>
#include <os/syscalls.h>
#include <os/msg/message.h>
#include <os/msg/init.h>

addr_t mapMem(addr_t addr, int device, size_t length, size_t offset, int flags);
addr_t unmapMem(addr_t addr, size_t length);
pid_t createPort(pid_t port, int flags);
pid_t destroyPort(pid_t port);
int registerDriver(int major, int numDevices, int type, size_t blockSize);
int unregisterDriver(int major);

addr_t mapMem(addr_t addr, int device, size_t length, size_t offset, int flags)
{
  msg_t msg =
  {
    .subject = MAP_MEM,
    .recipient = INIT_SERVER_TID
  };

  msg_t replyMsg;
  struct MapRequest *request = (struct MapRequest *)&msg.data;
  struct MapReply *reply = (struct MapReply *)&replyMsg.data;

  request->addr = addr;
  request->device = device;
  request->length = length;
  request->offset = offset;
  request->flags = flags;

  return (sys_call(&msg, &replyMsg, 1) == ESYS_OK
          && replyMsg.subject == REPLY_OK) ? reply->addr : NULL;
}

addr_t unmapMem(addr_t addr, size_t length)
{
  msg_t msg =
  {
    .subject = UNMAP_MEM,
    .recipient = INIT_SERVER_TID
  };

  msg_t replyMsg;
  struct UnmapRequest *request = (struct UnmapRequest *)&msg.data;

  request->addr = addr;
  request->length = length;

  return (sys_call(&msg, &replyMsg, 1) == ESYS_OK
           && replyMsg.subject == REPLY_OK) ? 0 : -1;
}

pid_t createPort(pid_t pid, int flags)
{
  msg_t msg =
  {
    .subject = CREATE_PORT,
    .recipient = INIT_SERVER_TID
  };

  msg_t replyMsg;
  struct CreatePortRequest *request = (struct CreatePortRequest *)&msg.data;
  struct CreatePortReply *reply = (struct CreatePortReply *)&replyMsg.data;

  request->pid = pid;
  request->flags = flags;

  return (sys_call(&msg, &replyMsg, 1) == ESYS_OK
           && replyMsg.subject == REPLY_OK) ? reply->pid : NULL_PID;
}

pid_t destroyPort(pid_t port)
{
  msg_t msg =
  {
    .subject = DESTROY_PORT,
    .recipient = INIT_SERVER_TID
  };

  msg_t replyMsg;
  struct DestroyPortRequest *request = (struct DestroyPortRequest *)&msg.data;

  request->port = port;

  return (sys_call(&msg, &replyMsg, 1) == ESYS_OK
           && replyMsg.subject == REPLY_OK) ? 0 : -1;
}

int registerServer(int type)
{
  msg_t msg =
  {
    .subject = REGISTER_SERVER,
    .recipient = INIT_SERVER_TID
  };

  msg_t replyMsg;
  struct RegisterServerRequest *request = (struct RegisterServerRequest *)&msg.data;

  request->type = type;

  return (sys_call(&msg, &replyMsg, 1) == ESYS_OK
           && replyMsg.subject == REPLY_OK) ? 0 : -1;
}

int unregisterServer(void)
{
  msg_t msg =
  {
    .subject = UNREGISTER_SERVER,
    .recipient = INIT_SERVER_TID
  };

  msg_t replyMsg;

  return (sys_call(&msg, &replyMsg, 1) == ESYS_OK
           && replyMsg.subject == REPLY_OK) ? 0 : -1;
}

int registerName(const char *name)
{
  msg_t msg =
  {
    .subject = REGISTER_NAME,
    .recipient = INIT_SERVER_TID,
  };

  if(!name)
    return -1;

  msg_t replyMsg;
  struct RegisterNameRequest *request = (struct RegisterNameRequest *)&msg.data;

  strncpy(request->name, name, sizeof request->name);

  return (sys_call(&msg, &replyMsg, 1) == ESYS_OK
           && replyMsg.subject == REPLY_OK) ? 0 : -1;
}

tid_t lookupName(const char *name)
{
  msg_t msg =
  {
    .subject = LOOKUP_NAME,
    .recipient = INIT_SERVER_TID
  };

  if(!name)
    return -1;

  msg_t replyMsg;
  struct LookupNameRequest *request = (struct LookupNameRequest *)&msg.data;
  struct LookupNameReply *reply = (struct LookupNameReply *)&msg.data;

  strncpy(request->name, name, sizeof request->name);

  return (sys_call(&msg, &replyMsg, 1) == ESYS_OK
           && replyMsg.subject == REPLY_OK) ? reply->tid : NULL_TID;
}

int unregisterName(const char *name)
{
  msg_t msg =
  {
    .subject = UNREGISTER_NAME,
    .recipient = INIT_SERVER_TID
  };

  if(!name)
    return -1;

  msg_t replyMsg;
  struct UnregisterNameRequest *request = (struct UnregisterNameRequest *)&msg.data;

  strncpy(request->name, name, sizeof request->name);

  return (sys_call(&msg, &replyMsg, 1) == ESYS_OK
           && replyMsg.subject == REPLY_OK) ? 0 : -1;
}

/*
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
int registerDevice(int major, int numDevices,
  unsigned long blockLen, int flags);

// Returns a device associated with a device major number
int lookupDevMajor(unsigned char major, struct DeviceRecord *record);

int exec( char *filename, char *args );
*/

/*
int registerFs( const char *name, size_t name_len, struct Filesystem *fsInfo );


int lookupFsName( const char *name, size_t name_len, struct Filesystem *fs );
*/

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

/*
int mapMem( void *phys, void *virt, int numPages, int flags )
{
  int retval;

  msg_t inMsg =
  { .subject = MAP_MEM,
    .recipient = INIT_SERVER,
    .data = {
      .i32 = { (int)phys, (int)virt, (int)numPages, (int)flags, 0 }
    }
  };

  msg_t outMsg;

  retval = sys_call(&inMsg, &outMsg, 1);
  return retval < 0 ? retval : outMsg.subject;
}

int allocatePages( void *address, int numPages )
{
  return mapMem((void *)NULL_PADDR, address, numPages, MEM_FLG_ALLOC);
}

int mapTid( tid_t tid, rspid_t pool_id )
{
  msg_t inMsg =
  { .subject = MAP_TID,
    .recipient = INIT_SERVER,
    .data = {
      .i32 = { (int)tid, (int)pool_id, 0, 0, 0 }
    }
  };

  msg_t outMsg;

  int retval = sys_call(&inMsg, &outMsg, 1);
  return retval < 0 ? -1 : outMsg.subject;
}
*/

/*
int createShmem( shmid_t shmid, unsigned pages, struct MemRegion *region,
                bool ro_perm )
{
  volatile struct GenericReq *req;
  volatile struct Message msg;
  volatile struct GenericMsgHeader *header;

  header = (struct GenericMsgHeader *)msg.data;
  req = (struct GenericReq *)header->data;

  if( region == NULL )
    return -1;

  header->type = CREATE_SHM;
  header->seq = 0;
  header->status = GEN_STAT_OK;

  req->arg[0] = (int)shmid;
  req->arg[1] = (int)pages;
  req->arg[2] = (int)ro_perm;
  req->arg[3] = (int)region->start;
  req->arg[4] = (int)region->length;

  msg.length = sizeof *req + sizeof *header;
  msg.protocol = MSG_PROTO_GENERIC;

  if( sendMsg( INIT_SERVER, (struct Message *)&msg, MSG_TIMEOUT ) < 0 )
    return -1;

  if( receiveMsg( INIT_SERVER, (struct Message *)&msg, MSG_TIMEOUT ) < 0 )
    return -1;

  if( header->status == GEN_STAT_OK )
    return 0;
  else
    return -1;
}

int attachShmemReg( shmid_t shmid, struct MemRegion *region )
{
  volatile struct GenericReq *req;
  volatile struct Message msg;
  volatile struct GenericMsgHeader *header;

  header = (struct GenericMsgHeader *)msg.data;
  req = (struct GenericReq *)header->data;

  if( region == NULL )
    return -1;

  header->type = ATTACH_SHM_REG;
  header->seq = 0;
  header->status = GEN_STAT_OK;

  req->arg[0] = (int)shmid;
  req->arg[1] = (int)region->start;
  req->arg[2] = (int)region->length;

  msg.length = sizeof *req + sizeof *header;
  msg.protocol = MSG_PROTO_GENERIC;

  if( sendMsg( INIT_SERVER, (struct Message *)&msg, MSG_TIMEOUT ) < 0 )
    return -1;

  if( receiveMsg( INIT_SERVER, (struct Message *)&msg, MSG_TIMEOUT ) < 0 )
    return -1;

  if( header->status == GEN_STAT_OK )
    return 0;
  else
    return -1;
}

int registerName(const char *name, size_t len)
{
  if(name == NULL || len > 16)
    return -1;
  
  int in_args[5] = { REGISTER_NAME, 0, 0, 0, 0 };
  int out_args[5];

  strncpy((char *)&in_args[1], name, len);

  int result = sys_call(INIT_SERVER, in_args, out_args, 1);
  return result < 0 ? result : out_args[0];
}

tid_t lookupName( const char *name, size_t len )
{
  if(name == NULL || len > 16)
    return -1;

  int in_args[5] = { LOOKUP_NAME, 0, 0, 0 };
  int  out_args[5];

  strncpy((char *)&in_args[1], name, len);

  int result = sys_call(INIT_SERVER, in_args, out_args, 1);
  return result < 0 ? NULL_TID : (pid_t)out_args[0];
}
*/

/*
int registerDevice(int major, int numDevices, unsigned long blockLen, int flags)
{
  msg_t inMsg =
  { .subject = DEV_REGISTER,
    .recipient = INIT_SERVER,
    .data = {
      .i32 = { (int)major, (int)numDevices, (int)blockLen, (int)flags, 0 }
    }
  };

  msg_t outMsg;

  int result = sys_call(&inMsg, &outMsg, 1);
  return result < 0 ? -1 : outMsg.subject;
}

int lookupDevMajor(unsigned char major, struct DeviceRecord *record)
{
  if(record == NULL)
    return -1;

  msg_t inMsg =
  { .subject = DEV_LOOKUP_MAJOR,
    .recipient = INIT_SERVER,
    .data = {
      .i32 = { (int)major, 0, 0, 0, 0 }
    }
  };

  msg_t outMsg;

  int result = sys_call(&inMsg, &outMsg, 1);

  if(result == 0)
  {
    if(record)
    {
      record->numDevices = outMsg.data.i32[1];
      record->blockLen = (unsigned long)outMsg.data.i32[2];
      record->flags = outMsg.data.i32[3];
    }
  }

  return result < 0 ? -1 : outMsg.subject;
}
*/
/*
int lookupDevName( const char *name, size_t name_len, struct Device *device )
{
  volatile struct NameLookupReq *devReq;
  volatile struct GenericMsgHeader *header;
  volatile struct Message msg;

  header = (struct GenericMsgHeader *)msg.data;
  devReq = (struct NameLookupReq *)header->data;

  if( device == NULL || name == NULL || name_len > N_MAX_NAME_LEN )
    return -1;

  header->type = DEV_LOOKUP_NAME;
  header->seq = 0;
  header->status = GEN_STAT_OK;

  devReq->name_len = name_len;
  strncpy((char *)devReq->name, name, name_len);

  msg.length = sizeof *devReq + sizeof *header;
  msg.protocol = MSG_PROTO_GENERIC;

  if( sendMsg( INIT_SERVER, (struct Message *)&msg, MSG_TIMEOUT ) < 0 )
    return -1;

  if( receiveMsg( INIT_SERVER, (struct Message *)&msg, MSG_TIMEOUT ) < 0 )
    return -1;

  if( header->status == GEN_STAT_OK )
  {
    *device = *(struct Device *)header->data;
    return 0;
  }
  else
    return -1;
}

int registerFs( const char *name, size_t name_len, struct Filesystem *fsInfo )
{
  struct RegisterNameReq *devReq;
  volatile struct DevMgrReply *reply;
  volatile struct Message msg;

  header = (struct GenericMsgHeader *)msg.data;
  req = (struct GenericReq *)header->data;

  if( name == NULL || name_len > N_MAX_NAME_LEN || fsInfo == NULL)
    return -1;

  header->req_type = DEV_REGISTER;
  devReq->name_len = name_len;
  strncpy(devReq->name, name, name_len);
  req->entry.fs = *fsInfo;
  req->name_type = FS_NAME;

  msg.length = sizeof *req;
  msg.protocol = MSG_PROTO_DEVMGR;

  if( sendMsg( INIT_SERVER, (struct Message *)&msg, MSG_TIMEOUT ) < 0 )
    return -1;

  if( receiveMsg( INIT_SERVER, (struct Message *)&msg, MSG_TIMEOUT ) < 0 )
    return -1;

  return reply->reply_status;
}

int lookupFsName( const char *name, size_t name_len, struct Filesystem *fs )
{
  struct NameLookupReq *req;
  volatile struct DevMgrReply *reply;
  volatile struct Message msg;

  header = (struct GenericMsgHeader *)msg.data;
  req = (struct GenericReq *)header->data;

  if( fs == NULL || name == NULL || name_len < N_MAX_NAME_LEN )
    return -1;

  req->req_type = DEV_LOOKUP_NAME;
  req->name_len = name_len;
  strncpy(req->name, name, name_len);

  msg.length = sizeof *req;
  msg.protocol = MSG_PROTO_DEVMGR;

  if( sendMsg( INIT_SERVER, (struct Message *)&msg, MSG_TIMEOUT ) < 0 )
    return -1;

  if( receiveMsg( INIT_SERVER, (struct Message *)&msg, MSG_TIMEOUT ) < 0 )
    return -1;

  if( reply->reply_status == REPLY_SUCCESS )
    *fs = reply->entry.fs;

  return reply->reply_status;
}


// XXX: Will break if filename or args is too long for a message

int exec( char *filename, char *args )
{
  volatile struct ExecReq *req;
  volatile struct Message msg;
  volatile struct GenericMsgHeader *header;

  header = (struct GenericMsgHeader *)msg.data;
  req = (struct ExecReq *)header->data;

  if( !filename || !args )
    return -1;

  header->type = EXEC;
  header->seq = 0;
  header->status = GEN_STAT_OK;

  req->nameLen = strlen(filename);
  req->argsLen = strlen(args);

  memcpy((void *)req->data, filename, req->nameLen);
  memcpy((void *)(req->data + req->nameLen), args, req->argsLen);

  msg.length = sizeof *req + sizeof *header + req->argsLen + req->nameLen;
  msg.protocol = MSG_PROTO_GENERIC;

  if( sendMsg( INIT_SERVER, (struct Message *)&msg, MSG_TIMEOUT ) < 0 )
    return -1;

  if( receiveMsg( INIT_SERVER, (struct Message *)&msg, MSG_TIMEOUT ) < 0 )
    return -1;

  if( header->status == GEN_STAT_OK )
    return 0;
  else
    return -1;
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

  if( sendMsg( INIT_SERVER, (struct Message *)&msg, MSG_TIMEOUT ) < 0 )
    return -1;

  buffer = malloc(sizeof(struct VfsListArgs) + req->pathLen);

  if( !buffer )
    return -1;

  args = (struct VfsListArgs *)(buffer + req->pathLen);

  args->maxEntries = maxEntries;
  memcpy( buffer, path, req->pathLen );

  if( sendLong( INIT_SERVER, buffer, sizeof(*args) + req->pathLen, MSG_TIMEOUT ) < 0 )
  {
    free(buffer);
    return -1;
  }

  free(buffer);

  if( receiveMsg( INIT_SERVER, (struct Message *)&msg, MSG_TIMEOUT ) < 0 )
    return -1;

  if( *(int *)msg.data > 0 )
  {
    if( receiveLong( INIT_SERVER, attrib, *(int *)msg.data * sizeof(struct FileAttributes), MSG_TIMEOUT ) < 0 )
      return -1;
  }

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

  if( sendMsg( INIT_SERVER, (struct Message *)&msg, MSG_TIMEOUT ) < 0 )
    return -1;

  buffer = malloc(sizeof(struct VfsGetAttribArgs) + req->pathLen);

  if( !buffer )
    return -1;

  args = (struct VfsGetAttribArgs *)(buffer + req->pathLen);

  memcpy( buffer, path, req->pathLen );

  if( sendLong( INIT_SERVER, buffer, sizeof(*args) + req->pathLen, MSG_TIMEOUT ) < 0 )
  {
    free(buffer);
    return -1;
  }

  free(buffer);

  if( receiveMsg( INIT_SERVER, (struct Message *)&msg, MSG_TIMEOUT ) < 0 )
    return -1;

  if( *(int *)msg.data == 0 )
  {
    if( receiveLong( INIT_SERVER, attrib, sizeof(struct FileAttributes), MSG_TIMEOUT ) < 0 )
      return -1;
  }

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

  if( sendMsg( INIT_SERVER, (struct Message *)&msg, MSG_TIMEOUT ) < 0 )
    return -1;

  buf = malloc(sizeof(struct VfsReadArgs) + req->pathLen);

  if( !buf )
    return -1;

  args = (struct VfsReadArgs *)(buf + req->pathLen);

  args->offset = offset;
  args->length = bytes;

  memcpy( buf, path, req->pathLen );

  if( sendLong( INIT_SERVER, buf, sizeof(*args) + req->pathLen, MSG_TIMEOUT ) < 0 )
  {
    free(buf);
    return -1;
  }

  free(buf);

  if( receiveMsg( INIT_SERVER, (struct Message *)&msg, MSG_TIMEOUT ) < 0 )
    return -1;

  if( *(int *)msg.data > 0 )
  {
    if( receiveLong( INIT_SERVER, buffer, *(int *)msg.data, MSG_TIMEOUT ) < 0 )
      return -1;
  }

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

  if( sendMsg( INIT_SERVER, (struct Message *)&msg, MSG_TIMEOUT ) < 0 )
    return -1;

  buffer = malloc(sizeof(struct MountArgs) + pathLen);

  if( !buffer )
    return -1;

  mountArgs = (struct MountArgs *)(buffer + pathLen);

  memcpy( buffer, path, pathLen );

  mountArgs->device = device;
  mountArgs->fsLen = fsLen;
  memcpy(mountArgs->fs, fs, fsLen);
  mountArgs->flags = flags;

  if( sendLong( INIT_SERVER, buffer, sizeof(struct MountArgs) + pathLen, MSG_TIMEOUT ) < 0 )
  {
    free(buffer);
    return -1;
  }

  free(buffer);

  if( receiveMsg( INIT_SERVER, (struct Message *)&msg, MSG_TIMEOUT ) < 0 )
    return -1;

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

  if( sendMsg( INIT_SERVER, (struct Message *)&msg, MSG_TIMEOUT ) < 0 )
    return -1;

  if( sendLong( INIT_SERVER, (void *)path, pathLen, MSG_TIMEOUT ) < 0 )
    return -1;

  if( receiveMsg( INIT_SERVER, (struct Message *)&msg, MSG_TIMEOUT ) < 0 )
    return -1;

  return *(int *)msg.data;
}

int changeIoPerm( unsigned start, unsigned stop, int set )
{
  volatile struct GenericReq *req;
  volatile struct GenericMsgHeader *header;
  volatile struct Message msg;
  int status;

  header = (volatile struct GenericMsgHeader *)msg.data;
  req = (volatile struct GenericReq *)header->data;

  header->type = CHANGE_IO_PERM;
  header->seq = 0;
  header->status = GEN_STAT_OK;

  req->arg[0] = (int)start;
  req->arg[1] = (int)stop;
  req->arg[2] = (int)set;

  msg.length = sizeof *req + sizeof *header;
  msg.protocol = MSG_PROTO_GENERIC;

  if( sendMsg( INIT_SERVER, (struct Message *)&msg, MSG_TIMEOUT ) < 0 )
    return -1;

  if( receiveMsg( INIT_SERVER, (struct Message *)&msg, MSG_TIMEOUT ) < 0 )
    return -1;

  if( header->status == GEN_STAT_OK )
    return 0;
  else
    return -1;
}
*/
