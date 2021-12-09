#ifndef DEVICE_REGISTRAR
#define DEVICE_REGISTRAR

#include "../type.h"
#include <oslib.h>

#define DEV_NUM(major, minor)	((((major) & 0xFFFF) << 16) | ((minor) & 0xFFFF))

#define N_MAX_NAME_LEN 		16
#define N_MAX_NAMES		(MAX_DEVICES + MAX_FILESYSTEMS)
#define MAX_DEVICES 		256
#define MAX_FILESYSTEMS		32
#define MAX_MOUNT_ENTRIES	32

// #define DEV_REGISTER		0
// #define DEV_LOOKUP_MAJOR	1
// #define DEV_LOOKUP_NAME		2

#define DEV_REG_SERVER		30

#define FLG_CHAR_DEV           0
#define FLG_BLOCK_DEV          1
#define FLG_NO_CACHE           0
#define FLG_WRITE_THRU         4
#define FLG_WRITE_BACK         8

/* Maybe use a linked list instead... */

struct DeviceRecord
{
  int numDevices;
  unsigned int blockLen;
  int flags;
};

struct Device
{
//  char name[MAX_DEV_NAME_LEN + 1];
  unsigned char numDevices;
  pid_t owner;
  unsigned long int blockLen;
  int flags : 31;
  int used : 1;
};

extern struct Device devices[MAX_DEVICES];

/*
struct Filesystem
{
  tid_t ownerTID;
  unsigned long flags;
} filesystems[MAX_FILESYSTEMS];
*/

union Entry
{
  struct Device device;
//  struct Filesystem fs;
};

#endif
