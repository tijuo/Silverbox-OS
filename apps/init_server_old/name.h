#ifndef NAME_H

#define NAME_H

#include "../../include/type.h"
#include <oslib.h>
#include <os/device.h>
#include <os/os_types.h>
#include <os/vfs.h>

#define MAX_NAME_RECS	256
#define MAX_NAME_LEN	18

union _NameEntry
{
  struct Device device;
  struct VFS_Filesystem fs;
  tid_t tid;
};

enum _NameType { THREAD, DEVICE,  FS };

struct NameRecord
{
  char name[MAX_NAME_LEN];
  size_t name_len;
  enum _NameType name_type;
  union _NameEntry entry;
};

SBAssocArray threadNames, deviceNames, fsNames, deviceTable, fsTable;

int _registerDevice(int major, struct Device *device);
struct Device * _unregisterDevice(unsigned char major);
struct Device *lookupDeviceMajor(int major);

#endif /* NAME_H */
