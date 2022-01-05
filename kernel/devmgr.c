#include <kernel/error.h>
#include <kernel/bits.h>
#include <kernel/memory.h>
#include <kernel/devmgr.h>
#include <drivers/video.h>
#include <drivers/pseudo.h>
#include <drivers/ramdisk.h>

#define NUM_DEV_MAJOR 256
#define DEV_VALID   0x80000000u

static struct driver driver_records[NUM_DEV_MAJOR];

int device_register_char(const char *name, int major, int flags, const struct driver_callbacks *callbacks) {
  return device_register(name, major, CLEAR_FLAG(flags, DEV_BLK), 1, callbacks);
}

int device_register(const char *name, int major, int flags, size_t block_size,
  const struct driver_callbacks *callbacks) {
  if(major >= NUM_DEV_MAJOR)
    RET_MSG(E_FAIL, "Invalid device major number.");
  else if(IS_FLAG_SET(driver_records[major].flags, DEV_VALID))
    RET_MSG(E_FAIL, "Major number has already been registered.");

  struct driver *new_driver = &driver_records[major];

  new_driver->major = major;
  new_driver->flags = flags;
  kstrncpy(new_driver->name, name, sizeof new_driver->name);

  if(!callbacks->create || !callbacks->read || !callbacks->write || !callbacks->get
    || !callbacks->set || !callbacks->destroy) {
    RET_MSG(E_FAIL, "All device callbacks must be non-NULL");
  }

  new_driver->callbacks = *callbacks;

  new_driver->block_size = IS_FLAG_SET(flags, DEV_BLK) ? block_size : 1;

  if(new_driver->block_size == 0)
    RET_MSG(E_FAIL, "Block size cannot be zero for block devices.");

  SET_FLAG(new_driver->flags, DEV_VALID);

  return E_OK;
}

int device_unregister(int major) {
  if(major >= NUM_DEV_MAJOR)
    RET_MSG(E_FAIL, "Invalid device major number.");
  else if(IS_FLAG_CLEAR(driver_records[major].flags, DEV_VALID))
    RET_MSG(E_FAIL, "Major number hasn't been registered yet");

  CLEAR_FLAG(driver_records[major].flags, DEV_VALID);

  return E_OK;
}

long int device_create(int major, int id, const void *params, int flags) {
  if(major < NUM_DEV_MAJOR && IS_FLAG_SET(driver_records[major].flags, DEV_VALID)) {
    return driver_records[major].callbacks.create(&driver_records[major], id, params, flags);
  }
  else
    return DEV_NO_EXIST;
}

long int device_get(int major, int id, ...) {
  if(major < NUM_DEV_MAJOR && IS_FLAG_SET(driver_records[major].flags, DEV_VALID)) {
    va_list list;

    va_start(list, major);
    long int result = driver_records[major].callbacks.get(&driver_records[major], id, list);
    va_end(list);

    return result;
  }
  else
    return DEV_NO_EXIST;
}

long int device_set(int major, int id, ...) {
  if(major < NUM_DEV_MAJOR && IS_FLAG_SET(driver_records[major].flags, DEV_VALID)) {
    va_list list;

    va_start(list, major);
    long int result = driver_records[major].callbacks.set(&driver_records[major], id, list);
    va_end(list);

    return result;
  }
  else
    return DEV_NO_EXIST;
}

long int device_read(int major, int minor, void *buf, size_t len, long int offset, int flags) {
  if(major < NUM_DEV_MAJOR && IS_FLAG_SET(driver_records[major].flags, DEV_VALID)) {
    if(IS_FLAG_SET(driver_records[major].flags, DEV_NO_READ))
      return DEV_PERM;
    else
      return driver_records[major].callbacks.read(&driver_records[major], minor, buf, len, offset, flags);
  }
  else
    return DEV_NO_EXIST;
}

long int device_write(int major, int minor, const void *buf, size_t len, long int offset, int flags) {
  if(major < NUM_DEV_MAJOR && IS_FLAG_SET(driver_records[major].flags, DEV_VALID)) {
    if(IS_FLAG_SET(driver_records[major].flags, DEV_NO_WRITE))
      return DEV_PERM;
    else
      return driver_records[major].callbacks.write(&driver_records[major], minor, buf, len, offset, flags);
  }
  else
    return DEV_NO_EXIST;
}

long int device_destroy(int major, int id) {
  if(major < NUM_DEV_MAJOR && IS_FLAG_SET(driver_records[major].flags, DEV_VALID)) {
    return driver_records[major].callbacks.destroy(&driver_records[major], id);
  }
  else
    return DEV_NO_EXIST;
}

int device_init(void) {
  pseudo_init();
  ramdisk_init();
  video_init();

  return E_OK;
}
