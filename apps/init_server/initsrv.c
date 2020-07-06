#include <oslib.h>
#include <string.h>
#include <stdlib.h>
#include <os/services.h>
#include <os/message.h>
#include <os/dev_interface.h>
#include <os/device.h>
#include <os/multiboot.h>

#include "paging.h"
#include "name.h"
#include "resources.h"
#include "shmem.h"

#define MSG_TIMEOUT		3000

/* Malloc works here as long as 'pager_addr_space' exists and is correct*/

extern int init(multiboot_info_t *info, addr_t resdStart, addr_t resdLen,
         addr_t discStart, addr_t discLen);
extern void print( char * );
extern void printInt( int n );

//extern void *list_malloc( size_t );
//extern void list_free( void * );

//extern struct ListType shmem_list;

//int extendHeap( unsigned pages );

static void handle_message(void);
/*static int handle_devmgr_request( tid_t sender, void *request,
   struct DevMgrReply *reply_msg );*/

/*extern int handleVfsRequest(tid_t sender, struct FsReqHeader *req, char *inBuffer,
size_t inBytes, char **outBuffer, size_t *outBytes);*/

//extern int loadElfFile(char *, char *);

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

  if( flags & MEM_FLG_LAZY )	// XXX: I wonder how a lazy COW would work...
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
/*
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
*/

static void handle_message( void )
{
  int result = 0;
  pid_t sender=NULL_PID;
  int in_args[5];
  int out_args[5] = { 0, 0, 0, 0, 0 };

  if(sys_receive(INIT_SERVER, &sender, in_args, 1) != ESYS_OK)
    return;

  switch(in_args[0])
  {
    case MAP_MEM:
      result = doMapMem( (void *)in_args[1], (void *)in_args[2],
                         (unsigned)in_args[3], in_args[4], sender );
      break;
    case MAP_TID:
      result = doMapTid( (tid_t)in_args[1], (rspid_t)in_args[2], sender );
      break;
    case DEV_REGISTER:
    {
      struct Device inDevice;

      inDevice.numDevices = in_args[2];
      inDevice.blockLen = in_args[3];
      inDevice.owner = sender;
      inDevice.flags = in_args[4];

      result = _registerDevice(in_args[1], &inDevice);
      break;
    }
    case DEV_LOOKUP_MAJOR:
    {
      struct Device *device = lookupDeviceMajor(in_args[1]);

      result = (device == NULL ? -1 : 0);

      if(device != NULL)
      {
        out_args[1] = (int)device->owner;
        out_args[2] = device->numDevices;
        out_args[3] = device->blockLen;
        out_args[4] = device->flags;
      }

      break;
    }
    case DEV_UNREGISTER:
    {
      struct Device *device = _unregisterDevice(in_args[1]);
      result = (device == NULL ? -1 : 0);
      break;
    }
    case EXCEPTION_MSG:
    {
      if(sender == NULL_PID)
        result = handle_exception(in_args);
      else
        result = -1;

      break;
    }
    default:
      result = -1;
      break;
  }

  out_args[0] = result;

  if(sender != NULL_PID)
    sys_send(INIT_SERVER, sender, out_args, 0);
/*
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
  }
#endif /* 0 */
}

//extern int registerFAT(void);

int main( multiboot_info_t *info, addr_t resdStart, addr_t resdLen,
          addr_t discStart, addr_t discLen )
{
  print("Initial server started.\n");

  if( init(info, &resdStart, &resdLen, &discStart, &discLen) )
    return 1;

  //registerFAT();

  while(1)
    handle_message();

  return 0;
}
