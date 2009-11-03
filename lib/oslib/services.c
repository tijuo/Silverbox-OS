#include <oslib.h>
#include <os/services.h>
#include <os/message.h>
#include <os/region.h>
#include <string.h>
#include <os/dev_interface.h>
#include <os/device.h>

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
/* XXX: There needs to be a way to handle reply messages. */

int mapMem( void *phys, void *virt, int numPages, int flags )
{
  struct GenericReq *req;
  struct Message msg;

  req = (struct GenericReq *)msg.data;

  req->request = MAP_MEM;
  req->arg[0] = (int)phys;
  req->arg[1] = (int)virt;
  req->arg[2] = (int)numPages;
  req->arg[3] = flags;

  msg.length = sizeof *req;
  msg.protocol = MSG_PROTO_GENERIC;

  while( __send( INIT_SERVER, &msg, 0 ) == 2 );
  while( __receive( INIT_SERVER, &msg, 0 ) == 2 );

  return req->arg[0];
}

int allocatePages( void *address, int numPages )
{
  volatile struct GenericReq *req;
  volatile struct Message msg;
  int status;

  req = (volatile struct GenericReq *)msg.data;

  req->request = ALLOC_MEM;
  req->arg[0] = (int)address;
  req->arg[1] = (int)numPages;

  msg.length = sizeof *req;
  msg.protocol = MSG_PROTO_GENERIC;

  while( (status=__send( INIT_SERVER, (struct Message *)&msg, 0 )) == 2 );

  if( status < 0 )
    return -1;

  while( __receive( INIT_SERVER, (struct Message *)&msg, 0 ) == 2 );

  return req->arg[0];
}

int mapTid( tid_t tid, void *addr_space )
{
  volatile struct GenericReq *req;
  volatile struct Message msg;

  req = (volatile struct GenericReq *)msg.data;

  req->request = MAP_TID;
  req->arg[0] = (int)tid;
  req->arg[1] = (int)addr_space;

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

int registerName( char *name, size_t len )
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

tid_t lookupName( char *name, size_t len )
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

int registerDevice( char *name, size_t name_len, struct Device *deviceInfo )
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

int lookupDevName( char *name, size_t name_len, struct Device *device )
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

int registerFs( char *name, size_t name_len, struct Filesystem *fsInfo )
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

int lookupFsName( char *name, size_t name_len, struct Filesystem *fs )
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


/*
  struct FileAttributes
  {
    int flags;
    long long timestamp; // number of microseconds since Jan. 1, 1950 CE
    long long size;
    unsigned char name_len;
    char name[255];  
  };

  struct FsReqMsg
  {
    int req;
    size_t pathLen;

    union {
      size_t nameLen;
      size_t dataLen;
    };
  };

int listDir( tid_t tid, char *path, size_t pathLen, 
             struct FileAttributes *entries, size_t maxEntries )
{
  struct FsReqMsg *req;
  struct FsReplyMsg *reply;
  struct Message msg;
  int ret;
  size_t bytes_recv = 0, bytes_to_recv = maxEntries * sizeof( struct FileAttributes );
  size_t bufferLen;

  req = (struct FsReqMsg *)msg.data;
  reply = (struct FsReplyMsg *)msg.data;

  if( path == NULL || pathLen == 0 || tid == NULL_TID )
    return -1;

  req->req = LIST_DIR;
  //strncpy( req->path, path, pathLen );
  req->pathLen = pathLen;
  req->dataLen = maxEntries * sizeof(struct FileAttributes);

  msg.length = sizeof *req;
  msg.protocol = MSG_PROTO_FS;

  while( (ret=__send( tid, &msg, 0 )) == 2 );

  if( ret < 0 )
    return -1;

  while( (ret=__receive( tid, (struct Message *)&msg, 0 )) == 2 );

  if( ret < 0 || reply->fail || reply->bufferLen == 0 )
    return -1;

  bufferLen = reply->bufferLen;

  while( pathLen )
  {
    _send( tid, path, pathLen > bufferLen ? bufferLen : pathLen, 0 );
    path += pathLen > bufferLen ? bufferLen : pathLen;
    pathLen -= pathLen > bufferLen ? bufferLen : pathLen;
  }

  while( num_recv < bytes_to_recv )
  {
    size_t diff = (bytes_to_recv - num_recv);

    num_recv += _receive( tid, entries, diff > bufferLen ? bufferLen : diff, 0 );
    entries = (struct FileAttributes *)((unsigned)entries + (diff > bufferLen ? 
                 bufferLen : diff));
  }

  return num_recv;
}

int readFile( tid_t tid, char *path, size_t pathLen, unsigned offset,
              void *buffer, size_t bytes )
{
  struct FsReqMsg *req;
  struct FsReplyMsg *reply;
  struct Message msg;
  size_t bufferLen;
  size_t num_recv;
  int ret;

  req = (struct FsReqMsg *)msg.data;
  reply = (struct FsReplyMsg *)msg.data;

  if( path == NULL || pathLen == 0 || tid == NULL_TID )
    return -1;

  req->req = READ_FILE;
  //strncpy( req->path, path, pathLen );
  req->pathLen = pathLen;
  req->dataLen = bytes;
  req->offset = offset;

  msg.length = sizeof *req;
  msg.protocol = MSG_PROTO_FS;

  while( (ret=__send( tid, &msg, 0 )) == 2 );

  if( ret < 0 )
    return -1;

  while( (ret=__receive( tid, (struct Message *)&msg, 0 )) == 2 );

  if( ret < 0 || reply->fail || reply->bufferLen == 0 )
    return -1;

  bufferLen = reply->bufferLen;

  while( pathLen )
  {
    _send( tid, path, pathLen > bufferLen ? bufferLen : pathLen, 0 );
    path += pathLen > bufferLen ? bufferLen : pathLen;
    pathLen -= pathLen > bufferLen ? bufferLen : pathLen;
  }

  while( num_recv < bytes )
  {
    size_t diff = bytes - num_recv;

    num_recv += _receive( tid, buffer, diff > bufferLen ? bufferLen : diff, 0 );
    buffer = (void *)((unsigned)buffer + (diff > bufferLen ? bufferLen : diff));
  }

  return num_recv;
}

int writeFile( tid_t tid, char *path, size_t pathLen, unsigned offset,
              void *buffer, size_t bytes )
{
  struct FsReqMsg *req;
  struct Message msg;
  int ret;

  req = (struct FsReqMsg *)msg.data;
  reply = (struct FsReplyMsg *)msg.data;

  if( path == NULL || pathLen == 0 || tid == NULL_TID )
    return -1;

  req->req = WRITE_FILE;
  //strncpy( req->path, path, pathLen );
  req->pathLen = pathLen;
  req->dataLen = bytes;

  msg.length = sizeof *req;
  msg.protocol = MSG_PROTO_FS;

  while( (ret=__send( tid, &msg, 0 )) == 2 );

  if( ret < 0 )
    return -1;

  while( (ret=__receive( tid, (struct Message *)&msg, 0 )) == 2 );

  if( ret < 0 || reply->fail || reply->bufferLen == 0 )
    return -1;

  bufferLen = reply->bufferLen;

  while( pathLen )
  {
    _send( tid, path, pathLen > bufferLen ? bufferLen : pathLen, 0 );
    path += pathLen > bufferLen ? bufferLen : pathLen;
    pathLen -= pathLen > bufferLen ? bufferLen : pathLen;
  }

  while( num_sent < bytes_to_send )
  {
    size_t diff = (bytes_to_send - num_sent);

    num_sent += _receive( tid, entries, diff > bufferLen ? bufferLen : diff, 0 );
    entries = (struct FileAttributes *)((unsigned)entries + (diff > bufferLen ? 
                 bufferLen : diff));
  }

  return num_sent;
}

int getAttributes( tid_t tid, char *path, size_t pathLen, 
                   struct FileAttributes *attrib )
{
  struct FsReqMsg *req;
  struct Message msg;
  int ret;

  req = (struct FsReqMsg *)msg.data;
  reply = (struct FsReplyMsg *)msg.data;

  if( path == NULL || pathLen == 0 || tid == NULL_TID || attrib == NULL )
    return -1;

  req->req = GET_ATTRIB;
  //strncpy( req->path, path, pathLen );
  req->pathLen = pathLen;
  req->dataLen = sizeof *attrib;

  msg.length = sizeof *req;
  msg.protocol = MSG_PROTO_FS;

  while( (ret=__send( tid, &msg, 0 )) == 2 );

  if( ret < 0 )
    return -1;

  while( (ret=__receive( tid, (struct Message *)&msg, 0 )) == 2 );

  if( ret < 0 || reply->fail || reply->bufferLen == 0 )
    return -1;

  bufferLen = reply->bufferLen;

  while( pathLen )
  {
    _send( tid, path, pathLen > bufferLen ? bufferLen : pathLen, 0 );
    path += pathLen > bufferLen ? bufferLen : pathLen;
    pathLen -= pathLen > bufferLen ? bufferLen : pathLen;
  }

  while( num_recv < bytes_to_recv )
  {
    size_t diff = (bytes_to_recv - num_recv);

    num_recv += _receive( tid, attrib, diff > bufferLen ? bufferLen : diff, 0 );
    attrib = (struct FileAttributes *)((unsigned)attrib + (diff > bufferLen ? 
                 bufferLen : diff));
  }

  return num_recv;
}
*/
