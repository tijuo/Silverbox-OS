#ifndef RAMDISK
#define RAMDISK

#define RAMDISK_MAJOR       2
#define RAMDISK_NAME        "ramdisk"
#define RAMDISK_ID          0
#define RAMDISK_IMAGE       0x40000000

#include <stdlib.h>

struct ramdisk_create_params {
  void *start_addr;
  size_t block_count;
  size_t block_size;
};

int ramdisk_init(void);

#endif
