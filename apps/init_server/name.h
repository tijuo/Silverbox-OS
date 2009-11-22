#ifndef NAME_H
#define NAME_H

#include <types.h>
#include <oslib.h>
#include <os/device.h>

#define MAX_NAME_RECS	256
#define MAX_NAME_LEN	18

union _NameEntry
{
  struct Device device;
//  struct Filesystem fs;
  tid_t tid;
};

enum _NameType { DEVICE,/* FS,*/ TID };

struct NameRecord
{
  char name[MAX_NAME_LEN];
  size_t name_len;
  enum _NameType name_type;
//  tid_t tid;
  union _NameEntry entry;
  
};

struct Device *lookupDeviceMajor(int major);
int _registerName(char *name, size_t len, enum _NameType type, void *data);
struct NameRecord *_lookupName(char *name, size_t len, enum _NameType type);

#endif /* NAME_H */
