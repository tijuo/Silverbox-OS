#include <stdlib.h>

void *kmalloc(size_t size);
void *kcalloc(size_t count, size_t elem_size);
void *krealloc(void *mem, size_t new_size);
void kfree(void *mem);

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
  return free(mem);
}
