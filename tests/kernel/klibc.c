#include <string.h>
#include <stdlib.h>
#include <kernel/memory.h>

void *kmemcpy(void * restrict dest, const void * restrict src, size_t size) {
  return memcpy(dest, src, size);
}

void *kmemset(void *base, int value, size_t len) {
  return memset(base, value, len);
}

void *kmalloc(size_t bytes, UNUSED size_t align) {
  return malloc(bytes);
}

void kfree(void *ptr) {
  free(ptr);
}
