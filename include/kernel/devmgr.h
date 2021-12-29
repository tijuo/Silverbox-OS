#ifndef KERNEL_DEVMGR_H
#define KERNEL_DEVMGR_H

#include <util.h>
#include <stdlib.h>

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

#define DEV_OK        0
#define DEV_FAIL      -1
#define DEV_NO_EXIST  -2

struct driver_callbacks {
  long int (*create)(int id, const void *params, int flags);
  long int (*read)(int minor, void *buf, size_t len, long int offset, int flags);
  long int (*write)(int minor, const void *buf, size_t len, long int offset, int flags);
  long int (*get)(int id, ...);
  long int (*set)(int id, ...);
  long int (*destroy)(int id);
};

NON_NULL_PARAMS int device_register_char(const char *name, int major, int flags,
  const struct driver_callbacks *callbacks);
NON_NULL_PARAMS int device_register(const char *name, int major, int flags,
  size_t block_size, const struct driver_callbacks *callbacks);
int device_unregister(int major);
int driver_init(void);

#endif /* KERNEL_DEVMGR_H */
