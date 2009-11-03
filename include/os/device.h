#ifndef DEVICE_REGISTRAR
#define DEVICE_REGISTRAR

#include <types.h>
#include <oslib.h>

#define N_MAX_NAME_LEN 		12
#define N_MAX_NAMES		(MAX_DEVICES + MAX_FILESYSTEMS)
#define MAX_DEVICES 		256
#define MAX_FILESYSTEMS		32
#define MAX_MOUNT_ENTRIES	32

#define REGISTER_DEVICE	 	0
#define LOOKUP_DEV_MAJOR      	1
#define LOOKUP_DEV_NAME       	2

#define DEV_REGISTER		0
#define DEV_LOOKUP_MAJOR	1
#define DEV_LOOKUP_NAME		2

#define DEV_REG_SERVER		30

enum DeviceTypes { CHAR_DEV, BLOCK_DEV };
enum CacheTypes { NO_CACHE, WRITE_THRU, WRITE_BACK };

enum NameType { DEV_NAME, FS_NAME };

/* Maybe use a linked list instead... */

struct Device
{
//  char name[MAX_DEV_NAME_LEN + 1];
  unsigned char major;
  unsigned char numDevices;
  unsigned long dataBlkLen : 31;
  unsigned long used : 1;
  tid_t ownerTID;
  enum DeviceTypes type;
  enum CacheTypes cacheType;
} devices[MAX_DEVICES];

struct Filesystem
{
  tid_t ownerTID;
  unsigned long flags;
} filesystems[MAX_FILESYSTEMS];

union Entry
{
  struct Device device;
  struct Filesystem fs;
};

struct RegisterNameReq
{
  int req_type;

  enum NameType name_type;

  union Entry entry;

  char name[N_MAX_NAME_LEN];
  size_t name_len;
};

struct NameLookupReq
{
  int req_type;

  union
  {
    unsigned char major;

    struct
    {
      char name[N_MAX_NAME_LEN];
      size_t name_len;
    };
  };
};

struct DevMgrReply
{
  int reply_status;
  enum NameType type;

  union Entry entry;
};

#endif
