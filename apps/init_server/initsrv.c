#include <oslib.h>
#include <string.h>
#include <stdlib.h>
#include <os/services.h>
#include <os/message.h>
#include <os/dev_interface.h>
#include <os/device.h>

#include "paging.h"
#include "name.h"
#include "shmem.h"

/* Malloc works here as long as 'pager_addr_space' exists and is correct*/

int init( int argc, char **argv );
extern void print( char * );
extern void *list_malloc( size_t );
extern void list_free( void * );

extern struct ListType shmem_list;

int extendHeap( unsigned pages );

void handle_message(void);

/* This extends the end of the heap by a certain number of pages. */

int extendHeap( unsigned pages )
{
  void *virt;

  if ( (unsigned)allocEnd % PAGE_SIZE != 0 )
    virt = (void *)( ( (unsigned)allocEnd & 0xFFFFF000 ) + PAGE_SIZE  );
  else
    virt = allocEnd;

  return mapMemRange( virt, pages );
}

/* Handles exceptions, memory allocations, i/o permissions,
   DMA, address spaces, exit messages. */

void handle_message( void )
{
  volatile struct Message msg;
  int result = 0;
  int result2;
  tid_t sender;

  while( (result2=__receive( NULL_TID, (struct Message *)&msg, 0 )) == 2 );

  sender = msg.sender;

  if( msg.protocol == MSG_PROTO_GENERIC )
  {
    volatile struct GenericReq *req = (volatile struct GenericReq *)msg.data;

    if( req->request == MSG_REPLY )
    {
      print("Received a reply?\n");
      return;
    }

    switch( req->request )
    {
      case MAP_MEM:
      {
        struct AddrRegion region;
        void *pdir;
        int flags = req->arg[3];

        pdir = lookup_tid(sender);

        if( pdir == NULL_PADDR )
        {
          print("Request map_mem(): Oops! TID ");
          printInt(sender);
          print(" is not registered.");

          result = -1;
          break;
        }

        region.virtRegion.start = req->arg[1];
        region.physRegion.start = req->arg[0];
        region.virtRegion.length = region.physRegion.length = 
           req->arg[2] * PAGE_SIZE;
        region.flags = MEM_MAP;

        if( flags & MEM_FLG_RO )
          region.flags |= MEM_RO;

        if( (flags & MEM_FLG_COW) && !(flags & MEM_FLG_ALLOC) ) // COW would imply !ALLOC
          region.flags |= MEM_RO | MEM_COW;

        if( flags & MEM_FLG_LAZY )	// I wonder how a lazy COW would work...
          region.flags |= MEM_LAZY;

        if( (flags & MEM_FLG_ALLOC) && !(flags & MEM_FLG_COW) ) // ALLOC would imply !COW
          region.flags &= ~MEM_MAP ;

        if( attach_mem_region(pdir, &region) != 0 )
        {
          print("attach_mem_region() failed.");
          result = -1;
          break;
        }

        if( !(flags & MEM_FLG_LAZY) && !(flags & MEM_FLG_ALLOC) )
        {
          _mapMem((void *)req->arg[0], (void *)req->arg[1], 
                    (unsigned)req->arg[2], req->arg[3], pdir); // doesn't return result
        }

        result = 0;
        break;
      }
      case MAP_TID:
      {
        if( req->arg[1] == (int)NULL_PADDR )
          result = attach_tid(lookup_tid(sender), (tid_t)req->arg[0]);
        else
          result = attach_tid((void *)req->arg[1], (tid_t)req->arg[0]);

        break;
      }
      case CREATE_SHM:
      {
        struct MemRegion region;

        region.start = req->arg[3];
        region.length = req->arg[4];

        result = init_shmem( (shmid_t)req->arg[0], sender, (unsigned)req->arg[1], (bool)req->arg[2] );

        if( result == 0 )
          result = shmem_attach( (shmid_t)req->arg[0], _lookup_tid(sender), &region );
        break;
      }
      case ATTACH_SHM_REG:
      {
        struct MemRegion region;

        region.start = req->arg[3];
        region.length = req->arg[4];

        result = shmem_attach( (shmid_t)req->arg[0], _lookup_tid(sender), &region );
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
      {
        struct NameRecord *record = _lookupName((char *)&req->arg[1],req->arg[0], THREAD);

        if( !record )
          result = _registerName((char *)&req->arg[1],req->arg[0], THREAD, &sender);
        else
          result = -1;
        break;
      }
      case LOOKUP_NAME:
      {
        struct NameRecord *record = _lookupName((char *)&req->arg[1], req->arg[0], THREAD);

        if( record )
          result = record->entry.tid;
        else
          result = -1;
        break;
      }
/*      case SET_IO_PERM:
        result = set_io_perm(req->argv[0], req->argv[1], argv[2], sender);
        break; */
      default:
        print("Received bad request!![");
        printHex(req->request);
        print(", ");
        printHex(sender);
        print("]\n");
        break;
    }

    if( result < 0 )
    {
      print("Generic request 0x"), printHex(req->request), print(" returned an error\n");
    }

    req->request = MSG_REPLY;
    req->arg[0] = result;
    msg.length = sizeof *req;

    while( __send( sender, (struct Message *)&msg, 0 ) == 2 );
  }
  else if( msg.protocol == MSG_PROTO_DEVMGR )
  {
    int status = REPLY_ERROR;
    int *type = (int *)msg.data;
    struct DevMgrReply *reply_msg = (struct DevMgrReply *)msg.data;

    switch( *type )
    {
      case DEV_REGISTER:
      {
        struct RegisterNameReq *req = (struct RegisterNameReq *)msg.data;
        int retval;
        enum _NameType name_type;
        req->entry.device.ownerTID = sender;

        if( req->name_type == DEV_NAME )
          name_type = DEVICE;
        /*else if( name_type == FS_NAME )
          name_type = FS;*/
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
        struct NameLookupReq *req = (struct NameLookupReq *)msg.data;
        struct Device *device = NULL;
        struct NameRecord *record = NULL;

        if( *type == DEV_LOOKUP_MAJOR )
          device = lookupDeviceMajor( req->major );
        else
          record = _lookupName( req->name, req->name_len, DEVICE );

        if( device == NULL && record == NULL )
          status = REPLY_ERROR;
        else
        {
          if( *type != DEV_LOOKUP_MAJOR )
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
    {
      print("Device request 0x"), printHex(*type), print(" returned an error\n");
    }

    reply_msg->reply_status = status;
    msg.length = sizeof *reply_msg;

    while( __send( sender, (struct Message *)&msg, 0 ) == -2 );
  }
  else
  {
    print("Invalid message protocol: ");
    print(toIntString(msg.protocol));
    print(" ");
    print(toIntString(sender));
    print(" ");
    print(toIntString(msg.length));
    print("\n");
    return;
  }
}

int main( int argc, char **argv )
{
  init(argc, argv);
  print("Initialized.\n");

  while(1)
    handle_message();

  return 0;
}
