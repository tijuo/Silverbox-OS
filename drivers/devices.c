#include <string.h>
#include <device.h>
#include <stdlib.h>
#include <oslib.h>
#include <os/message.h>
#include <os/services.h>
#include <dev_interface.h>
#include <os/signal.h>

#define DEVICE_MGR	"device_mgr"

#define DEV_MAX_NAMES	N_MAX_NAMES

struct NameEntry
{
  char name[N_MAX_NAME_LEN];
  size_t name_len;

  union Entry entry;
  enum NameType name_type;
} names[DEV_MAX_NAMES];
/*
struct MountEntry
{
  struct Filesystem *fs;
  short devNum;
  char *path;
  size_t pathLen;
  long flags;
} mountTable[MAX_MOUNT_ENTRIES];
*/
static unsigned num_names=0;
//static unsigned num_mount_entries=0;
static unsigned num_fs=0;

static int _register( tid_t tid, enum NameType name_type, union Entry *device );
static int registerDevName( tid_t tid, char *name, size_t name_len, 
                            enum NameType name_type, union Entry *device );
static struct Device *_lookupMajor( unsigned char major );
static union Entry *_lookupName( char *name, size_t len );

void handle_request(void);

static int registerDevName( tid_t tid, char *name, size_t name_len, 
                         enum NameType name_type, union Entry *entry )
{
  if( name == NULL || name_len > N_MAX_NAME_LEN || entry == NULL )
  {
    print("Null error1\n");

    if( name == NULL )
      print("Name\n");
    else if( entry == NULL )
      print("Entry\n");
    else
    {
      print("Name length ");
      print(toIntString(name_len));
      print("\n");
    }
    return -1;
  }
  if( num_names >= DEV_MAX_NAMES )
  {
    print("Too many names\n");
    return -1;
  }

  // TODO: Try to find a duplicate name here

  strncpy( names[num_names].name, name, name_len );
  names[num_names].name_len = name_len;

  names[num_names].entry = *entry;
  num_names++;

  return _register(tid, name_type, entry);
}

static int _register( tid_t tid, enum NameType name_type, union Entry *entry )
{
  if( name_type == DEV_NAME )
  {
    if( entry->device.numDevices == 0 || entry->device.dataBlkLen == 0 || 
        (unsigned char)entry->device.major >= MAX_DEVICES )
    {
      print("Null error\n");
      return -1;
    }

    if( entry->device.type != CHAR_DEV && entry->device.type != BLOCK_DEV )
    {
      print("Bad device type\n");
      return -1;
    }

    if( devices[entry->device.major].used )
    {
      print("Device already used\n");
      return 1;
    }

    devices[entry->device.major] = entry->device;
    devices[entry->device.major].used = 1;
    devices[entry->device.major].ownerTID = tid;

    return 0;
  }
  else
  {
    if( num_fs >= MAX_FILESYSTEMS )
    {
      print("Too many filesystems\n");
      return -1;
    }

    filesystems[num_fs] = entry->fs;
    filesystems[num_fs++].ownerTID = tid;

    return 0;
  }
}

static struct Device *_lookupMajor( unsigned char major )
{
  if( (unsigned char)major >= MAX_DEVICES )
    return NULL;

  if( !devices[major].used )
    return NULL;

  return &devices[major];
}

static union Entry *_lookupName( char *name, size_t len )
{
  if( name == NULL || len >= N_MAX_NAME_LEN )
    return NULL;

  for(unsigned i=0; i < N_MAX_NAMES; i++)
  {
    if( strncmp(names[i].name, name, len) == 0 )
      return &names[i].entry;
  }

  return NULL;
}

void handle_request()
{
  struct Message msg;
  int *type = (int *)msg.data;
  struct DevMgrReply *reply_msg = (struct DevMgrReply *)msg.data;
  int status = REPLY_ERROR;

  while( __receive(NULL_TID, &msg, 0) == -2);

  if( msg.protocol != MSG_PROTO_DEVMGR || msg.length < sizeof(int) )
    return;

  switch(*type)
  {
    case DEV_REGISTER:
    {
      struct RegisterNameReq *req = (struct RegisterNameReq *)msg.data;
      int retval = registerDevName(msg.sender, req->name, req->name_len, 
                     req->name_type, &req->entry);

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
      struct NameEntry *entry = NULL;

      if( *type == DEV_LOOKUP_MAJOR )
        device = _lookupMajor( req->major );
      else
        entry = _lookupName( req->name, req->name_len );

      if( device == NULL && entry == NULL )
        status = REPLY_ERROR;
      else
      {
        if( *type != DEV_LOOKUP_MAJOR )
        {
          reply_msg->entry = entry->entry;
          reply_msg->type = entry->name_type;
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
      print("Bad request\n");
      break;
  }

  tid_t sender = msg.sender;

  reply_msg->reply_status = status;
  msg.length = sizeof *reply_msg;

  while( __send( sender, &msg, 0 ) == -2 );
}

int main(void)
{
  struct Message msg;

//  __map(0xB8000, 0xB8000, 8);

  if(registerName(DEVICE_MGR, strlen(DEVICE_MGR)) != 0)
    return 1;

  while(1)
    handle_request();

  return 1;
}

