#include <oslib.h>
#include <string.h>
#include <stdlib.h>
#include <os/services.h>
#include <os/message.h>
#include <os/dev_interface.h>
#include <os/device.h>
#include <os/signal.h>

#include "paging.h"
#include "name.h"
#include "resources.h"
#include "shmem.h"

#define MSG_TIMEOUT		3000

/* Malloc works here as long as 'pager_addr_space' exists and is correct*/

extern int init( int argc, char **argv );
extern void print( char * );
//extern void *list_malloc( size_t );
//extern void list_free( void * );

//extern struct ListType shmem_list;

//int extendHeap( unsigned pages );

static void handle_message(void);
static int handle_generic_request( tid_t sender, struct GenericMsgHeader *header, struct Message *reply );
/*static int handle_devmgr_request( tid_t sender, void *request,
   struct DevMgrReply *reply_msg );*/

extern int handleVfsRequest(tid_t sender, struct FsReqHeader *req, char *inBuffer,
size_t inBytes, char **outBuffer, size_t *outBytes);

extern int loadElfFile(char *, char *);

/*
struct
{
  char *fName;
  struct RPC_Node *(*func)(struct RPC_Node *);
  unsigned arity;
} rpcFuncTable[] =;

struct RPC_Node *mapMemHandler(struct RPC_Node * )
{

}
*/

/* This extends the end of the heap by a certain number of pages. */
/*
int extendHeap( unsigned pages )
{
  void *virt;

  if ( (unsigned)allocEnd % PAGE_SIZE != 0 )
    virt = (void *)( ( (unsigned)allocEnd & 0xFFFFF000 ) + PAGE_SIZE  );
  else
    virt = allocEnd;

  return mapMemRange( virt, pages );
}
*/
/* Handles exceptions, memory allocations, i/o permissions,
   DMA, address spaces, exit messages. */

static int doMapMem( void *physStart, void *virtStart, unsigned pages,
int flags, tid_t sender )
{
  struct AddrRegion region;
  struct ResourcePool *pool = NULL;

  pool = lookup_tid(sender);

  if( pool == NULL )
  {
     print("Request map_mem(): Oops! TID ");
     printInt(sender);
     print(" is not registered.");

     return -1;
  }

  region.virtRegion.start = (int)virtStart;
  region.physRegion.start = (int)physStart;
  region.virtRegion.length = region.physRegion.length = pages * PAGE_SIZE;
  region.flags = REG_MAP;

  if( region.virtRegion.start == 0xF0000000 )
  { print("virt = 0xF0000000\n"); }

  if( flags & MEM_FLG_RO )
    region.flags |= REG_RO;

  if( (flags & MEM_FLG_COW) && !(flags & MEM_FLG_ALLOC) ) // COW would imply !ALLOC
    region.flags |= REG_RO | REG_COW;

  if( flags & MEM_FLG_LAZY )	// I wonder how a lazy COW would work...
    region.flags |= REG_LAZY;

  if( (flags & MEM_FLG_ALLOC) && !(flags & MEM_FLG_COW) ) // ALLOC would imply !COW
    region.flags &= ~REG_MAP ;

  if( attach_mem_region(&pool->addrSpace, &region) != 0 )
  {
    print("attach_mem_region() failed.");
    return -1;
  }

  if( !(flags & MEM_FLG_LAZY) && !(flags & MEM_FLG_ALLOC) )
    return _mapMem(physStart, virtStart, pages, flags, &pool->addrSpace);
  else
    return 0;
}

static int doMapTid( tid_t tid, rspid_t rspid, tid_t sender )
{
  if( rspid == NULL_RSPID )
    return attach_tid(lookup_tid(sender), tid);
  else
    return attach_tid(lookup_rspid(rspid), tid);
}

static int doRegisterName( size_t nameLen, char *nameStr, tid_t sender )
{
  if( !_lookupName(nameStr, nameLen, THREAD) )
    return _registerName(nameStr, nameLen, THREAD, &sender);
  else
    return -1;
}

static int doLookupName( size_t nameLen, char *nameStr )
{
  struct NameRecord *record = _lookupName(nameStr, nameLen, THREAD);

  if( record )
    return (int)record->entry.tid;
  else
    return -1;
}

static int doExec( char *execName, size_t nameLen, char *args, size_t argsLen )
{
  char *filename = NULL, *arguments = NULL;

  if( nameLen )
  {
    filename = malloc(nameLen+1);

    if( !filename )
      return -1;

    memcpy(filename, execName, nameLen);

    filename[nameLen] = '\0';
  }

  if( argsLen )
  {
    arguments = malloc(argsLen+1);

    if( !arguments )
      return -1;

    memcpy(arguments, args, argsLen);

    arguments[argsLen] = '\0';
  }

  return loadElfFile(filename, arguments);
}

static int handle_generic_request( tid_t sender, struct GenericMsgHeader *header, struct Message *reply )
{
  int result=-1;
  struct GenericReq *req = (struct GenericReq *)header->data;
  struct GenericMsgHeader *replyHeader = (struct GenericMsgHeader *)reply->data;
  void *replyData=NULL;
  size_t replyLen=0;
  tid_t tid;

  switch( header->type )
  {
    case MAP_MEM:
      result = doMapMem( (void *)req->arg[0], (void *)req->arg[1],
                         (unsigned)req->arg[2], req->arg[3], sender );
      break;
    case MAP_TID:
      result = doMapTid( (tid_t)req->arg[0], (rspid_t)req->arg[1], sender );
      break;
    case EXEC:
    {
      struct ExecReq *execReq = (struct ExecReq *)header->data;
      result = doExec(execReq->data, execReq->nameLen, execReq->data + execReq->nameLen, execReq->argsLen);
      break;
    }
      case CREATE_SHM:
      {
/*
        struct MemRegion region;

        region.start = req->arg[3];
        region.length = req->arg[4];

        result = init_shmem( (shmid_t)req->arg[0], sender, (unsigned)req->arg[1], (bool)req->arg[2] );

        if( result == 0 )
          result = shmem_attach( (shmid_t)req->arg[0], _lookup_tid(sender), &region );
*/
        break;
      }
      case ATTACH_SHM_REG:
      {
/*
        struct MemRegion region;

        region.start = req->arg[3];
        region.length = req->arg[4];

        result = shmem_attach( (shmid_t)req->arg[0], _lookup_tid(sender), &region );
*/
        break;
      }
      case DETACH_SHM_REG:
      {
 // detach_shared_mem( args->shmid, _lookup_tid(sender), &args->region );

        break;
      }
      case DELETE_SHM:
      {
  //delete_shared_mem( args->shmid, _lookup_tid(sender), &args->region );

        break;
      }
    case REGISTER_NAME:
      result = doRegisterName((size_t)req->arg[0], (char *)&req->arg[1], sender);
      break;
    case LOOKUP_NAME:
      result = doLookupName((size_t)req->arg[0], (char *)&req->arg[1]);

      if( result )
      {
        tid = (tid_t)result;

        replyLen = sizeof(tid_t);
        replyData = &tid;
        result = 0;
      }
      break;
    case UNREGISTER_NAME:
      break;
    case DEV_REGISTER:
    {
      struct RegisterNameReq *devReq = (struct RegisterNameReq *)header->data;
      int retval;

      devReq->entry.device.ownerTID = sender;

      result = _registerName(devReq->name, devReq->name_len, DEVICE, &devReq->entry);

      break;
    }
    case DEV_LOOKUP_NAME:
    case DEV_LOOKUP_MAJOR:
    {
      struct NameLookupReq *devReq = (struct NameLookupReq *)header->data;
      struct Device *device = NULL;
      struct NameRecord *record = NULL;

      if( header->type == DEV_LOOKUP_MAJOR )
        device = lookupDeviceMajor( (unsigned char)devReq->major );
      else
        record = _lookupName( devReq->name, devReq->name_len, DEVICE );

      if( device == NULL && record == NULL )
        result = 1;
      else
      {
        if( header->type == DEV_LOOKUP_NAME && record->name_type == DEVICE )
        {
          replyData = &record->entry.device;
          replyLen = sizeof record->entry.device;
        }
        else if( header->type == DEV_LOOKUP_MAJOR )
        {
          replyData = device;
          replyLen = sizeof *device;
        }

        result = 0;
      }
      break;
    }
    case DEV_UNREGISTER:
      break;
    case CHANGE_IO_PERM:
      result = __set_io_perm(req->arg[0], req->arg[1], req->arg[2], sender);
      break;
    default:
      print("Received bad generic request 0x");
      printHex(header->type);
      print(" from ");
      printInt(sender);
      print("\n");
      break;
  }

  reply->protocol = MSG_PROTO_GENERIC;

  if( result < 0 )
    replyHeader->status = GEN_STAT_ERROR;
  else if(result > 0 )
    replyHeader->status = GEN_STAT_FAIL;
  else
    replyHeader->status = GEN_STAT_OK;

  replyHeader->type = GEN_REPLY_TYPE | header->type;
  replyHeader->seq = 0;
  memcpy(replyHeader->data, replyData, replyLen);
  reply->length = sizeof *replyHeader + replyLen;

  return 0;
}
/*
static int handle_devmgr_request( tid_t sender, void *request,
   struct DevMgrReply *reply_msg )
{
  int type = *(int *)request;
  int status = REPLY_ERROR;

  switch( type )
  {
    case DEV_REGISTER:
    {
      struct RegisterNameReq *req = (struct RegisterNameReq *)request;
      int retval;
      enum _NameType name_type;

      req->entry.device.ownerTID = sender;

      if( req->name_type == DEV_NAME )
        name_type = DEVICE;
      / *else if( name_type == FS_NAME )
        name_type = FS;* /
      else
      {
        status = REPLY_ERROR;
        break;
      }

      retval = _registerName(req->name, req->name_len, name_type, &req->entry);

      if( retval > 0 )
        status = REPLY_FAIL;
      else if( retval == 0 )
        status = REPLY_SUCCESS;
      else
        status = REPLY_ERROR;

      break;
    }
    case DEV_LOOKUP_NAME:
    case DEV_LOOKUP_MAJOR:
    {
      struct NameLookupReq *req = (struct NameLookupReq *)request;
      struct Device *device = NULL;
      struct NameRecord *record = NULL;

      if( type == DEV_LOOKUP_MAJOR )
        device = lookupDeviceMajor( (unsigned char)req->major );
      else
        record = _lookupName( req->name, req->name_len, DEVICE );

      if( device == NULL && record == NULL )
        status = REPLY_ERROR;
      else
      {
        if( type != DEV_LOOKUP_MAJOR )
        {
          reply_msg->entry.device = record->entry.device;

          if( record->name_type == DEVICE )
            reply_msg->type = DEV_NAME;
          else
            status = REPLY_ERROR;
        }
        else
        {
          reply_msg->entry.device = *device;
          reply_msg->type = DEV_NAME;
        }
        status = REPLY_SUCCESS;
      }
      break;
    }
    default:
      break;
  }

  if( status != REPLY_SUCCESS )
    print("Device request 0x"), printHex(type), print(" returned an error\n");

  return status;
}
*/

volatile struct Message *msg = NULL;
volatile struct Message *replyMsg = NULL;

static void handle_message( void )
{
//  volatile struct Message *msg = NULL, *reply = NULL;
  int result = 0;
  tid_t sender;

  if( !(msg = malloc(sizeof(*msg))) )
    return;

  if( !(replyMsg = malloc(sizeof(*replyMsg))) )
  {
    free((void *)msg);
    return;
  }

  while(1)
  {
    if( receiveMsg( NULL_TID, (struct Message *)msg, -1 ) < 0 )
    {
      free((void *)msg);
      free((void *)replyMsg);
      return;
    }

    sender = msg->sender;

    if( msg->protocol == MSG_PROTO_GENERIC )
    {
      handle_generic_request( msg->sender, (struct GenericMsgHeader *)msg->data, (struct Message *)replyMsg );

      if( sendMsg( sender, (struct Message *)replyMsg, MSG_TIMEOUT ) < 0 )
        break;
    }
    else if( msg->protocol == MSG_PROTO_DEVMGR )
    {
      print("Obsolete protocol: Device Manager Protocol\n");
    }
    else if( msg->protocol == MSG_PROTO_VFS )
    {
      struct FsReqHeader *req = (struct FsReqHeader *)msg->data;
      char *inBuffer = NULL, *outBuffer=NULL;
      size_t outBytes=0;

      if( req->argLen + req->pathLen )
      {
        inBuffer = malloc( req->argLen + req->pathLen );

        if( receiveLong( sender, inBuffer, req->argLen + req->pathLen, MSG_TIMEOUT ) < 0 )
        {
          free(inBuffer);
          break;
        }
      }

      result = handleVfsRequest( sender, req, inBuffer, req->argLen + req->pathLen,
         &outBuffer, &outBytes );

      if( req->argLen + req->pathLen )
        free( inBuffer );

      msg->length = sizeof(int);
      *(int *)msg->data = result;

      if( sendMsg( sender, (struct Message *)msg, MSG_TIMEOUT ) < 0 )
      {
        if( outBytes )
          free( outBuffer );
        break;
      }

      if( outBytes )
      {
        if( sendLong( sender, outBuffer, outBytes, MSG_TIMEOUT ) < 0 )
        {
          free( outBuffer );
          break;
        }

        free( outBuffer );
      }
    }
  /*
    else if( msg.protocol == MSG_PROTO_RPC )
    {
      char *fName;
      struct RPC_Node *node, *retNode;
      uchar *rpc_str;
      uint64_t strLength;

      receiveRPC(&msg, &fName, &node);

      for( int i=0; i < sizeof rpcFuncTable / sizeof (struct RPC_Func); i++ )
      {
        if( strncmp(fName, rpcFuncTable[i].fName) == 0 )
        {
          retNode = rpcFuncTable[i].fName(node);
          break;
        }
      }

      rpc_delete_node(node);

      rpc_to_string(retNode, &rpc_str, &strLength);

      rpc_delete_node(retNode);

      sendRPCMessage(msg.sender, *fName, rpc_str, strLength, RPC_PROTO_RPC);
      free(fName);
      free(rpc_str);
    }
  */
    else
    {
      print("Invalid message protocol: ");
      print(toIntString(msg->protocol));
      print(" ");
      print(toIntString(sender));
      print(" ");
      print(toIntString(msg->length));
      print("\n");
      return;
    }
  }
}

extern int registerFAT(void);

int main( int argc, char **argv )
{
  init(argc, argv);
  registerFAT();

  while(1)
    handle_message();

  return 0;
}
