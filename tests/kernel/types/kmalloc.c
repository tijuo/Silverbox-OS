#include <stdlib.h>
#include <string.h>

void *kmalloc(size_t size);
void *kcalloc(size_t count, size_t elem_size);
void *krealloc(void *mem, size_t new_size);
void kfree(void *mem);
void kmemcpy(void *dest, void *src, size_t size);

void *kmalloc(size_t size) {
  return malloc(size);
}

void *kcalloc(size_t count, size_t elem_size) {
  return calloc(count, elem_size);
}

void *krealloc(void *mem, size_t new_size) {
  return realloc(mem, new_size);
}

void kfree(void *mem) {
  free(mem);
}

void kmemcpy(void *dest, void *src, size_t size) {
  memcpy(dest, src, size);
}

void kmemset(void *base, int value, size_t len) {
  memset(base, value, len);
}
