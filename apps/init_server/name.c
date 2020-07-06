#include <oslib.h>
#include <os/services.h>
#include <os/os_types.h>
#include <os/device.h>
#include <string.h>
#include "name.h"
#include <os/vfs.h>

static int registerThreadName(char *name, size_t len, tid_t tid);
static int registerFsName(char *name, size_t len, struct VFS_Filesystem *fs);

int _registerDevice(int major, struct Device *device);
struct Device * _unregisterDevice(unsigned char major);
struct Device *lookupDeviceMajor(int major);


static int _registerFs(struct VFS_Filesystem *fs);
static struct VFS_Filesystem *_unregisterFs(char *name, size_t len);


int _registerDevice(int major, struct Device *device)
{
  return sbAssocArrayInsert(&deviceTable, &major, sizeof(int),
           device, sizeof *device);
}

struct Device *_unregisterDevice(unsigned char major)
{
  struct Device *dev;

  if( sbAssocArrayRemove( &deviceTable, &major, sizeof major, (void **)&dev, NULL ) != 0 )
    return NULL;
  else
    return dev;
}

static int _registerFs(struct VFS_Filesystem *fs)
{
  if( !fs )
    return -1;

  if( sbAssocArrayInsert( &fsTable, fs->name, fs->nameLen, fs, sizeof *fs ) 
        != 0 )
  {
    return -1;
  }
  else
    return 0;
}

static struct VFS_Filesystem *_unregisterFs(char *name, size_t len)
{
  struct VFS_Filesystem *fs;

  if( sbAssocArrayRemove( &fsTable, name, len, (void **)&fs, NULL ) != 0 )
    return NULL;
  else
    return fs;
}

static int registerFsName(char *name, size_t len, struct VFS_Filesystem *fs)
{
  struct NameRecord *record;

  if( len > VFS_NAME_MAXLEN )
    return -1;

  record = malloc(sizeof *record);

  if( !record )
    return -1;

  memcpy(record->name, name, len);
  record->name_len = len;
  record->entry.fs = *fs;
  record->name_type = FS;

  if( sbAssocArrayInsert(&fsNames, name, len, record, sizeof *record) != 0 )
  {
    free(record);
    return -1;
  }

  return 0;
}

struct Device *lookupDeviceMajor(int major)
{
  struct Device *dev;

  if( sbAssocArrayLookup(&deviceTable, &major, sizeof major, (void **)&dev, NULL) != 0 )
    return NULL;
  else
    return dev;
}
