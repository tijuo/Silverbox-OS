#include <kernel/types/bitmap.h>
#include <ia32intrin.h>
#include <limits.h>

int bitmap_init_at(bitmap_t *restrict bitmap, size_t elements, void *restrict mem_region,
    bool set_bits) {

  if(elements > LONG_MAX)
    RET_MSG(E_FAIL, "Too many bits.");
  else if(elements == 0)
    RET_MSG(E_FAIL, "Bitmap must contain at least one bit.");

  bitmap->data = (uint64_t *)mem_region;
  bitmap->count = elements;

  memset(bitmap->data, set_bits ? 0xFF : 0, BITMAP_BYTES(elements));

  return E_OK;
}

int bitmap_init(bitmap_t *bitmap, size_t elements, bool set_bits) {
  void *buffer = kmalloc(BITMAP_BYTES(elements));

  if(!buffer)
    RET_MSG(E_FAIL, "Unable to allocate buffer for bitmap.");
  else if(IS_ERROR(bitmap_init_at(bitmap, elements, buffer, set_bits)))
  {
    kfree(buffer);
    return E_FAIL;
  }
  else
    return E_FAIL;
}

int bitmap_release(bitmap_t *bitmap) {
  kfree(buffer->data);

  buffer->data = NULL;
  buffer->count = 0;

  return E_OK;
}

long int bitmap_find(const bitmap_t *bitmap, bool set) {
  size_t i=0;

  while(i < BITMAP_ELEMS(bitmap->count) && bitmap->data[i] == (set ? UINT64_MAX : 0))
    i++;

  if(i == BITMAP_ELEMS(bitmap->count))
    return E_NOT_FOUND;
  else
    return (int)(i * sizeof(uint64_t) * 8 + __bsfq(set ? bitmap->data[i] : ~bitmap->data[i]));
}

long int bitmap_rfind(const bitmap_t *bitmap, bool set) {
  long int i = (long int)BITMAP_ELEMS(bitmap->count)-1;

  while(i >= 0 && bitmap->data[i] == (set ? UINT64_MAX : 0))
    i--;

  if(i < 0)
    return E_NOT_FOUND;
  else
    return (int)(i * sizeof(uint64_t) * 8 + __bsrq(set ? bitmap->data[i] : ~bitmap->data[i]));
}
