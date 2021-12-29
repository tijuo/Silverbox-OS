#include <drivers/ramdisk.h>
#include <kernel/types/vector.h>
#include <kernel/devmgr.h>
#include <kernel/mm.h>
#include <kernel/memory.h>
#include <kernel/bits.h>
#include <kernel/memory.h>

#define DEV_VALID   0x80000000

struct ramdisk_device {
  void *data;
  size_t block_size;
  size_t block_count;
  int flags;
};

static vector_t ramdisks;

static long int ramdisk_create(int id, const void *params, int flags) {
  switch(id) {
    case RAMDISK_ID: {
        const struct ramdisk_create_params *create_params = (const struct ramdisk_create_params *)params;
        struct ramdisk_device *dev = NULL;

        if(create_params->block_count == 0 || create_params->block_size == 0
            || (IS_FLAG_SET(flags, RAMDISK_IMAGE) && create_params->start_addr == NULL)) {
          return DEV_FAIL;
        }

        for(size_t i=0; i < vector_get_count(&ramdisks); i++) {
          struct ramdisk_device *d = (struct ramdisk_device *)vector_item(&ramdisks, i);

          if(IS_FLAG_CLEAR(d->flags, DEV_VALID)) {
            dev = d;
            break;
          }
        }

        if(dev == NULL) {
          dev = kcalloc(sizeof *dev, 1, 0);

          if(dev == NULL)
            return DEV_FAIL;
          else {
            if(vector_push_back(&ramdisks, dev) != DEV_OK) {
              kfree(dev);
              return DEV_FAIL;
            }

            dev = (struct ramdisk_device *)vector_item(&ramdisks, vector_get_count(&ramdisks)-1);
          }
        }

        dev->data = IS_FLAG_SET(flags, RAMDISK_IMAGE) ? create_params->start_addr : NULL;
        dev->block_count = create_params->block_count;
        dev->block_size = create_params->block_size;
        dev->flags = flags;

        if(IS_FLAG_CLEAR(flags, RAMDISK_IMAGE)) {
          dev->data = kcalloc(1, dev->block_count * dev->block_size, 0);

          if(dev->data == NULL)
            return DEV_FAIL;
        }

        SET_FLAG(dev->flags, DEV_VALID);

        return DEV_OK;
      }
      break;
    default:
      return DEV_FAIL;
  }
}

static long int ramdisk_read(int minor, void *buf, size_t len, long int offset, int flags) {
  switch(minor) {
    case RAMDISK_ID: {
        const struct ramdisk_device *dev;

        if((size_t)minor >= vector_get_count(&ramdisks))
          return DEV_NO_EXIST;

        dev = (const struct ramdisk_device *)vector_item(&ramdisks, (size_t)minor);

        if(IS_FLAG_CLEAR(dev->flags, DEV_VALID))
          return DEV_NO_EXIST;
        else if(IS_FLAG_SET(dev->flags, DEV_NO_READ))
          return DEV_FAIL;
        else {
          if((size_t)offset >= dev->block_count)
            offset = dev->block_count - 1;

          if(len >= dev->block_count)
            len = dev->block_count - offset;

          kmemcpy(buf, (void *)((uintptr_t)dev->data + offset * dev->block_size),
                                len * dev->block_size);
          return DEV_OK;
        }
      }
    default:
      return DEV_NO_EXIST;
  }
}

static long int ramdisk_write(int minor, const void *buf, size_t len, long int offset, int flags) {
  switch(minor) {
    case RAMDISK_ID: {
        const struct ramdisk_device *dev;

        if((size_t)minor >= vector_get_count(&ramdisks))
          return DEV_NO_EXIST;

        dev = (const struct ramdisk_device *)vector_item(&ramdisks, (size_t)minor);

        if(IS_FLAG_CLEAR(dev->flags, DEV_VALID))
          return DEV_NO_EXIST;
        else if(IS_FLAG_SET(dev->flags, DEV_NO_WRITE))
          return DEV_FAIL;
        else {
          if((size_t)offset >= dev->block_count)
            offset = dev->block_count - 1;

          if(len >= dev->block_count)
            len = dev->block_count - offset;

          kmemcpy((void *)((uintptr_t)dev->data + offset * dev->block_size), buf,
                           len * dev->block_size);
          return DEV_OK;
        }
      }
    default:
      return DEV_NO_EXIST;
  }
}

static long int ramdisk_get(int id, ...) {
  return DEV_FAIL;
}

static long int ramdisk_set(int id, ...) {
  return DEV_FAIL;
}

static long int ramdisk_destroy(int id) {
  struct ramdisk_device *dev;

  if((size_t)id >= vector_get_count(&ramdisks))
    return DEV_NO_EXIST;

  dev = (struct ramdisk_device *)vector_item(&ramdisks, (size_t)id);

  if(IS_FLAG_CLEAR(dev->flags, DEV_VALID))
    return DEV_NO_EXIST;

  CLEAR_FLAG(dev->flags, DEV_VALID);

  return DEV_OK;
}

struct driver_callbacks ramdisk_callbacks = {
  .create = ramdisk_create,
  .read = ramdisk_read,
  .write = ramdisk_write,
  .get = ramdisk_get,
  .set = ramdisk_set,
  .destroy = ramdisk_destroy
};

int ramdisk_init(void) {
  vector_init(&ramdisks, sizeof(struct ramdisk_device));

  return device_register(RAMDISK_NAME, RAMDISK_MAJOR, DEV_BLK | DEV_NOCACHE,
                         PAGE_SIZE, &ramdisk_callbacks);
}
