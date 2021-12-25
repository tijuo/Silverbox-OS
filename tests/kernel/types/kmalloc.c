#include <stdlib.h>
#include <string.h>

#ifdef GNUC
  #define UNUSED __attribute__((unused))
#else
  #define UNUSED
#endif /* GNUC */

void *kmalloc(size_t size, UNUSED size_t align);
void *kcalloc(size_t count, size_t elem_size, UNUSED size_t align);
void *krealloc(void *mem, size_t new_size, UNUSED size_t align);
void kfree(void *mem);
void kmemcpy(void *dest, void *src, size_t size);
void *kmemset(void *ptr, int value, size_t size);

void *kmalloc(size_t size, size_t align) {
  return malloc(size);
}

void *kmemset(void *ptr, int value, size_t size) {
  return memset(ptr, value, size);
}

void *kcalloc(size_t count, size_t elem_size, size_t align) {
  return calloc(count, elem_size);
}

void *krealloc(void *mem, size_t new_size, size_t align) {
  return realloc(mem, new_size);
}

void kfree(void *mem) {
  free(mem);
}

void kmemcpy(void *dest, void *src, size_t size) {
  memcpy(dest, src, size);
}
