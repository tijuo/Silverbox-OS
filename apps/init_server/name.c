#include <oslib.h>
#include <os/services.h>
#include <os/os_types.h>
#include <os/device.h>
#include <string.h>
#include "name.h"
#include "vfs.h"

static int registerThreadName(char *name, size_t len, tid_t tid);
static int registerFsName(char *name, size_t len, struct VFS_Filesystem *fs);
static int registerDeviceName(char *name, size_t len, struct Device *dev);

static int _registerDevice(struct Device *device);
static struct Device * _unregisterDevice(int major);
static int _registerFs(struct VFS_Filesystem *fs)
static struct VFS_Filesystem *_unregisterFs(char *name, size_t len)

struct Device *lookupDeviceMajor(int major);
int _registerName(char *name, size_t len, enum _NameType type, void *data);
struct NameRecord *_lookupName(char *name, size_t len, enum _NameType type);
int _unregisterName(char *name, size_t len, enum _NameType type);

static int _registerDevice(struct Device *device)
{
  if( !device )
    return -1;

  if( sbAssocArrayInsert( &deviceTable, &device->major, sizeof device->major,
        device, sizeof *device ) < 0 )
  {
    return -1;
  }
  else
    return 0;
}

static struct Device *_unregisterDevice(int major)
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

  if( sbAssocArrayRemove( &fsTable, name, len (void **)&fs, NULL ) != 0 )
    return NULL;
  else
    return fs;
}

int _registerName(char *name, size_t len, enum _NameType type, void *data)
{
  int ret = -1;
  struct NameRecord *record;

  if( !data )
    return ret;

  switch( type )
  {
    case THREAD:
      ret = registerThreadName(name, len, *(tid_t *)data);
      break;
    case DEVICE:
    {
      if( registerDeviceName(name, len, (struct Device *)data) != 0 )
        break;

      record = _lookupName(name, len, DEVICE);

      if( record )
        ret = _registerDevice(&record->entry.device);

      break;
    }
    case FS:
    {
      if( registerFsName(name, len, (struct Filesystem *)fs) != 0 )
        break;

      record = _lookupName(name, len, FS);

      if( record )
        ret = _registerFs(&record->entry.fs);

      break;
    }
    default:
      break;
  }

  return ret;
}

int _unregisterName(char *name, size_t len, enum _NameType type)
{
  struct NameRecord *record;
  SBAssocArray *array = NULL;

  switch( type )
  {
    case THREAD:
      array = &threadNames;
      break;
    case DEVICE:
      array = &deviceNames;
      break;
    case FS:
      array = &fsNames;
      break;
    default:
      break;
  }

  if( sbAssocArrayRemove(array, name, len, (void **)&record, NULL) != 0 )
    return -1;

  if( type == DEVICE )
    free(_unregisterDevice(record->entry.device.major));
  else if( type == FS )
    free(_unregisterFs(record->entry.fs.name, record->entry.fs.nameLen));

  free(record);

  return 0;
}

static int registerThreadName(char *name, size_t len, tid_t tid)
{
  struct NameRecord *record;

  if( len > MAX_NAME_LEN )
    return -1;

  record = malloc(sizeof *record);

  if( !record )
    return -1;

  memcpy(record->name, name, len);
  record->name_len = len;
  record->entry.tid = tid;
  record->name_type = THREAD;

  if( sbAssocArrayInsert(&threadNames, name, len, record, sizeof *record) != 0 )
  {
    free(record);
    return -1;
  }

  return 0;
}

static int registerDeviceName(char *name, size_t len, struct Device *dev)
{
  struct NameRecord *record;

  if( len > MAX_NAME_LEN )
    return -1;

  record = malloc(sizeof *record);

  if( !record )
    return -1;

  memcpy(record->name, name, len);
  record->name_len = len;
  record->entry.device = *dev;
  record->name_type = DEVICE;

  if( sbAssocArrayInsert(&deviceNames, name, len, record, sizeof *record) != 0 )
  {
    free(record);
    return -1;
  }

  return 0;
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

  if( sbAssocArrayInsert(&fsName, name, len, record, sizeof *record) != 0 )
  {
    free(record);
    return -1;
  }

  return 0;
}


struct NameRecord *_lookupName(char *name, size_t len, enum _NameType type)
{
  struct NameRecord *record;
  SBAssocArray *array = NULL;

  switch(type)
  {
    case THREAD:
      array = &threadNames;
      break;
    case DEVICE:
      array = &deviceNames;
      break;
    case FS:
      array = &fsNames;
      break;
    default:
      break;
  }

  if( !array )
    return NULL;

  if( sbAssocArrayLookup(array, name, len, (void **)&record, NULL) != 0 )
    return NULL;
  else
    return record;
}

struct Device *lookupDeviceMajor(int major)
{
  struct Device *dev;

  if( sbAssocArrayLookup(&deviceTable, &major, sizeof major, (void **)&dev, NULL) != 0 )
    return NULL;
  else
    return dev;
}

/*
struct NameEntry *lookupDeviceName(char *name, size_t len)
{
  for(int i=0; i < numDeviceNames; i++)
  {
    if( strncmp(deviceNames[i].name, name, len) == 0 )
      return &deviceNames[i].entry.device;
  }

  return NULL;
}
*/
