#include <oslib.h>
#include <os/services.h>
#include <os/device.h>
#include <string.h>
#include "name.h"

static struct NameRecord threadNames[MAX_NAME_RECS], 
              deviceNames[MAX_NAME_RECS]/*, fsNames[MAX_NAME_RECS]*/;
static unsigned numThreadNames = 0, /*numFsNames = 0,*/ numDeviceNames = 0;

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

struct Device *lookupDeviceMajor(int major);
int _registerName(char *name, size_t len, enum _NameType type, void *data);
struct NameRecord *_lookupName(char *name, size_t len, enum _NameType type);

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
int _registerName(char *name, size_t len, enum _NameType type, void *data)
{
  switch( type )
  {
    case TID:
      return registerThreadName(name, len, *(tid_t *)data);
    case DEVICE:
      return registerDeviceName(name, len, (struct Device *)data);
    //case FS:
    //  return registerFsName(name, len, *(struct Filesystem *)fs);
    default:
      return -1;
  }
  return -1;
}

int registerThreadName(char *name, size_t len, tid_t tid)
{
  if( len > MAX_NAME_LEN )
    return -1;
  else if( numThreadNames >= MAX_NAME_RECS )
    return -1;

  memcpy(threadNames[numThreadNames].name, name, len);

  threadNames[numThreadNames].name_len = len;
  threadNames[numThreadNames].entry.tid = tid;

  numThreadNames++;

  return 0;
}

int registerDeviceName(char *name, size_t len, struct Device *dev)
{
  if( len > MAX_NAME_LEN )
    return -1;
  else if( numDeviceNames >= MAX_NAME_RECS )
    return -1;

  memcpy(deviceNames[numDeviceNames].name, name, len);

  deviceNames[numDeviceNames].name_len = len;
  deviceNames[numDeviceNames].entry.device = *dev;

  numDeviceNames++;
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
  switch(type)
  {
    case TID:
    {
      for(int i=0; i < numThreadNames; i++)
      {
        if( strncmp(threadNames[i].name, name, len) == 0 )
          return &threadNames[i];
      }
      break;
    }
    case DEVICE:
    {
      for(int i=0; i < numDeviceNames; i++)
      {
        if( strncmp(deviceNames[i].name, name, len) == 0 )
          return &deviceNames[i];
      }
      break;
    }
/*
    case FS:
    {
      for(int i=0; i < numfsNames; i++)
      {
        if( strncmp(fsNames[i].name, name, len) == 0 )
          return &fsNames[i];
      }
      break;
    }
*/
    default:
      return NULL;
  }

  return NULL;
}

struct Device *lookupDeviceMajor(int major)
{
  for(int i=0; i < numDeviceNames; i++)
  {
    if( major == deviceNames[i].entry.device.major )
      return &deviceNames[i].entry.device;
  }

  return NULL;
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
