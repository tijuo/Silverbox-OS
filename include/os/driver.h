#ifndef OS_DRIVER_H
#define OS_DRIVER_H

#include <os/ostypes/string.h>
#include <os/ostypes/hashtable.h>

typedef short int major_t;
typedef short int minor_t;

#define INVALID_MAJOR   ((major_t)-1)
#define INVALID_MINOR   ((minor_t)-1)

typedef struct {
    major_t major;
    minor_t minor;
} DeviceId;

typedef struct {
    minor_t (*create)(int flags);
    long int (*read)(minor_t minor, long int offset, void *buffer, long int buffer_len, int flags);
    long int (*write)(minor_t minor, long int offset, const void *buffer, long int buffer_len, int flags);
    int (*destroy)(minor_t minor);
} DriverOps;

typedef struct {
    major_t major;
    String name;
    DriverOps ops;
} Driver;

extern DriverOps DEFAULT_DRIVER_OPS;

#endif /* OS_DRIVER_H */