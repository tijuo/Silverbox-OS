#ifndef KERNEL_DEVMGR_H
#define KERNEL_DEVMGR_H

#include <util.h>
#include <stdlib.h>
#include <stdarg.h>
#include <kernel/error.h>

#define DEV_BLK     0x00000001u
#define DEV_CHAR    0x00000000u

#define DEV_RAW       0x00000002u
#define DEV_CACHED    0u
#define DEV_NOCACHE   0x00000004u
#define DEV_APPEND    0x00000008u
#define DEV_STATELESS 0u
#define DEV_STATEFUL  0x00000010u
#define DEV_READ      0u
#define DEV_NO_READ   0x00000020u
#define DEV_WRITE     0u
#define DEV_NO_WRITE  0x00000040u

#define DEV_OK        E_OK
#define DEV_FAIL      E_FAIL
#define DEV_NO_EXIST  E_NOT_FOUND
#define DEV_PERM      E_PERM

struct driver;

struct driver_callbacks {
  long int (*create)(struct driver *this_obj, int id, const void *params, int flags);
  long int (*read)(struct driver *this_obj, int minor, void *buf, size_t len, long int offset, int flags);
  long int (*write)(struct driver *this_obj, int minor, const void *buf, size_t len, long int offset, int flags);
  long int (*get)(struct driver *this_obj, int id, va_list args);
  long int (*set)(struct driver *this_obj, int id, va_list args);
  long int (*destroy)(struct driver *this_obj, int id);
};

struct driver {
  char name[16];
  int major;
  int flags;
  size_t block_size;

  struct driver_callbacks callbacks;
};

NON_NULL_PARAMS int device_register_char(const char *name, int major, int flags,
  const struct driver_callbacks *callbacks);
NON_NULL_PARAMS int device_register(const char *name, int major, int flags,
  size_t block_size, const struct driver_callbacks *callbacks);
int device_unregister(int major);
long int device_create(int major, int id, const void *params, int flags);
long int device_get(int major, int id, ...);
long int device_set(int major, int id, ...);
long int device_read(int major, int minor, void *buf, size_t len, long int offset, int flags);
long int device_write(int major, int minor, const void *buf, size_t len, long int offset, int flags);
long int device_destroy(int major, int id);

int device_init(void);

#endif /* KERNEL_DEVMGR_H */
