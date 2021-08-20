#ifndef NAME_H
#define NAME_H

#include <types.h>
#include <oslib.h>
#include <os/device.h>
#include <os/os_types.h>
#include <os/vfs.h>
#include <os/ostypes/sbhash.h>
#include <os/msg/message.h>
#include <os/msg/init.h>

#define MAX_NAME_RECS	256

union _NameEntry
{
  struct Device device;
  struct VFS_Filesystem fs;
  tid_t tid;
};

enum _NameType { THREAD, DEVICE,  FS };

struct NameRecord
{
  char name[MAX_NAME_LEN+1];
  enum _NameType nameType;
  union _NameEntry entry;
};

int _registerDevice(int major, struct Device *device);
struct Device * _unregisterDevice(int major);
struct Device *lookupDeviceMajor(int major);

void nameRegister(msg_t *request, msg_t *response);
void nameLookup(msg_t *request, msg_t *response);
void nameUnregister(msg_t *request, msg_t *response);

#endif /* NAME_H */
