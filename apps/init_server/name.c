#include <oslib.h>
#include <os/services.h>
#include <os/os_types.h>
#include <os/device.h>
#include <string.h>
#include "name.h"

/*
static struct NameRecord threadNames[MAX_NAME_RECS], 
              deviceNames[MAX_NAME_RECS]/*, fsNames[MAX_NAME_RECS]* /;
static unsigned numThreadNames = 0, / *numFsNames = 0,* / numDeviceNames = 0;
*/

/*
int registerThreadName(char *name, size_t len, tid_t tid);
tid_t lookupThreadName(char *name, size_t len);
int registerDeviceName(char *name, size_t len, struct Device *dev);
struct Device *lookupDeviceMajor( char major );
struct _NameEntry *lookupDeviceName( char *name, size_t len );
*/

static int registerThreadName(char *name, size_t len, tid_t tid);
//static int registerFsName(char *name, size_t len, struct Filesystem *fs);
static int registerDeviceName(char *name, size_t len, struct Device *dev);

static int _registerDevice(struct Device *device);
static struct Device * _unregisterDevice(int major);

struct Device *lookupDeviceMajor(int major);
int _registerName(char *name, size_t len, enum _NameType type, void *data);
struct NameRecord *_lookupName(char *name, size_t len, enum _NameType type);
int _unregisterName(char *name, size_t len, enum _NameType type);

/*
tid_t lookupThreadName(char *name, size_t len)
{
  for(int i=0; i < numThreadNames; i++)
  {
    if( strncmp(threadNames[i].name, name, len) == 0 )
      return threadNames[i].tid;
  }

  return NULL_TID;
}
*/

static int _registerDevice(struct Device *device)
{
  if( !device )
    return -1;

  if( sbAssocArrayInsert( &deviceTable, &device->major, sizeof device->major,
        device, sizeof device ) < 0 )
  {
    return -1;
  }
  else
    return 0;
}

static struct Device * _unregisterDevice(int major)
{
  struct Device *dev;

  if( sbAssocArrayRemove( &deviceTable, &major, sizeof major, (void **)&dev, NULL ) != 0 )
    return NULL;
  else
    return dev;
}

int _registerName(char *name, size_t len, enum _NameType type, void *data)
{
  int ret = -1;

  if( !data )
    return ret;

  switch( type )
  {
    case THREAD:
      ret = registerThreadName(name, len, *(tid_t *)data);
      break;
    case DEVICE:
    {
      struct Device *dev = (struct Device *)data;
      struct NameRecord *record;

      if( registerDeviceName(name, len, dev) != 0 )
        break;

      record = _lookupName(name, len, DEVICE);

      if( record )
        ret = _registerDevice(&record->entry.device);

      break;
    }
    //case FS:
    //  return registerFsName(name, len, *(struct Filesystem *)fs);
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
    default:
      break;
  }

  if( sbAssocArrayRemove(array, name, len, (void **)&record, NULL) != 0 )
    return -1;

  if( type == DEVICE )
    _unregisterDevice(record->entry.device.major);

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
/*
int registerFsName(char *name, size_t len, struct Filesystem *fs)
{
  if( len > MAX_NAME_LEN )
    return -1;
  else if( numFsNames >= MAX_NAME_RECS )
    return -1;

  memcpy(deviceNames[numFsNames].name, name, len);

  fsNames[numFsNames].name_len = len;
  fsNames[numFsNames].entry.fs = *fs;

  numFsNames++;
  return 0;
}
*/

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
/*
    case FS:
    {
      break;
    }
*/
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
