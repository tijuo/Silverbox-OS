#include <kernel/devmgr.h>
#include <kernel/memory.h>
#include <drivers/pseudo.h>

static long int pseudo_create(struct driver *this_obj, int id, const void *params, int flags) {
  switch(id) {
    case NULL_MINOR:
    case RAND_MINOR:
      return DEV_FAIL;
    default:
      return DEV_NO_EXIST;
  }
}

static long int pseudo_read(struct driver *this_obj, int minor, void *buf, size_t bytes, long int offset, int flags) {
  switch(minor) {
    case NULL_MINOR:
      kmemset(buf, 0, bytes);
      return bytes;
    case RAND_MINOR:
      return DEV_FAIL;
    default:
      return DEV_NO_EXIST;
  }
}

static long int pseudo_write(struct driver *this_obj, int minor, const void *buf, size_t bytes, long int offset, int flags) {
  switch(minor) {
    case NULL_MINOR:
    case RAND_MINOR:
      return DEV_FAIL;
    default:
      return DEV_NO_EXIST;
  }
}

static long int pseudo_get(struct driver *this_obj, int id, va_list args) {
  switch(id) {
    case NULL_MINOR:
    case RAND_MINOR:
      return DEV_FAIL;
    default:
      return DEV_NO_EXIST;
  }
}

static long int pseudo_set(struct driver *this_obj, int id, va_list args) {
  switch(id) {
    case NULL_MINOR:
    case RAND_MINOR:
      return DEV_FAIL;
    default:
      return DEV_NO_EXIST;
  }
}

static long int pseudo_destroy(struct driver *this_obj, int id) {
  switch(id) {
    case NULL_MINOR:
    case RAND_MINOR:
      return DEV_FAIL;
    default:
      return DEV_NO_EXIST;
  }
}

struct driver_callbacks pseudo_callbacks = {
  .create = pseudo_create,
  .read = pseudo_read,
  .write = pseudo_write,
  .get = pseudo_get,
  .set = pseudo_set,
  .destroy = pseudo_destroy
};

int pseudo_init(void) {
  return device_register(PSEUDO_NAME, PSEUDO_MAJOR, DEV_CHAR, 0, &pseudo_callbacks);
}
