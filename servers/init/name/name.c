#include <oslib.h>
#include <os/services.h>
#include <os/os_types.h>
#include <os/device.h>
#include <string.h>
#include "name.h"
#include <os/vfs.h>
#include <os/msg/init.h>

sbhash_t threadNames, deviceNames, fsNames, deviceTable, fsTable;

static int registerThreadName(char *name, tid_t tid);
static int registerFsName(char *name, struct VFS_Filesystem *fs);

int _registerDevice(int major, struct Device *device);
struct Device * _unregisterDevice(int major);
struct Device *lookupDeviceMajor(int major);

static int _registerFs(struct VFS_Filesystem *fs);
static struct VFS_Filesystem *_unregisterFs(char *name);

struct ThreadMapping
{
  char name[MAX_NAME_LEN+1];
  tid_t tid;
};

int _registerDevice(int major, struct Device *device)
{
  char *m = malloc(5);

  if(!m)
    return -1;

  itoa(major, m, 10);

  return sbHashInsert(&deviceTable, m, device);
}

struct Device *_unregisterDevice(int major)
{
  struct Device *dev;
  char m[5];
  char *storedName;

  itoa(major, m, 10);

  // must remove key

  if(sbHashRemovePair(&deviceTable, m, &storedName, (void **)&dev) == 0)
  {
    free(storedName);
    return dev;
  }
  else
   return NULL;
}

static int _registerFs(struct VFS_Filesystem *fs)
{
  return (!fs || sbHashInsert(&fsTable, fs->name, fs) != 0) ? -1 : 0;
}

static struct VFS_Filesystem *_unregisterFs(char *name)
{
  struct VFS_Filesystem *fs;

  if(sbHashLookup(&fsTable, name, (void **)&fs) != 0)
    return NULL;

  return (sbHashRemove(&fsTable, name) == 0) ? fs : NULL;
}

static int registerFsName(char *name, struct VFS_Filesystem *fs)
{
  struct NameRecord *record;

  record = malloc(sizeof *record);

  if(!record)
    return -1;

  record->name[MAX_NAME_LEN] = '\0';
  strncpy(record->name, name, MAX_NAME_LEN);
  record->entry.fs = *fs;
  record->nameType = FS;

  if(sbHashInsert(&fsNames, name, record) != 0)
  {
    free(record);
    return -1;
  }

  return 0;
}

struct Device *lookupDeviceMajor(int major)
{
  struct Device *dev;
  char m[5];

  itoa(major, m, 10);

  return (sbHashLookup(&deviceTable, m, (void **)&dev) == 0) ? dev : NULL;
}

void nameRegister(msg_t *request, msg_t *response)
{
  struct RegisterNameRequest *nameRequest = (struct RegisterNameRequest *)&request->data;
  struct ThreadMapping *mapping = malloc(sizeof *mapping);

  mapping->name[MAX_NAME_LEN] = '\0';
  strncpy(mapping->name, nameRequest->name, MAX_NAME_LEN);
  mapping->tid = request->sender;

  int result = sbHashInsert(&threadNames, mapping->name, mapping);

  response->subject = (result == 0 ? RESPONSE_OK : RESPONSE_FAIL);
}

void nameLookup(msg_t *request, msg_t *response)
{
  struct LookupNameRequest *nameRequest = (struct LookupNameRequest *)&request->data;
  struct ThreadMapping *mapping;
  char name[MAX_NAME_LEN+1];

  name[MAX_NAME_LEN] = '\0';
  strncpy(name, nameRequest->name, MAX_NAME_LEN);

  if(sbHashLookup(&threadNames, name, (void **)&mapping) == 0)
  {
    struct LookupNameResponse *nameResponse = (struct LookupNameResponse *)&response->data;

    nameResponse->tid = mapping->tid;
    response->subject = RESPONSE_OK;
  }
  else
    response->subject = RESPONSE_FAIL;
}

void nameUnregister(msg_t *request, msg_t *response)
{
  struct UnregisterNameRequest *nameRequest = (struct UnregisterNameRequest *)&request->data;
  struct ThreadMapping *mapping;
  char name[MAX_NAME_LEN+1];

  name[MAX_NAME_LEN] = '\0';
  strncpy(name, nameRequest->name, MAX_NAME_LEN);

  if(sbHashRemovePair(&threadNames, name, NULL, (void **)&mapping) != 0)
  {
    response->subject = RESPONSE_FAIL;
  }
  else
  {
    free(mapping);
    response->subject = RESPONSE_OK;
  }
}

