#include <string.h>
#include <stdlib.h>
#include <kernel/memory.h>

void *kmemcpy(void * restrict dest, const void * restrict src, size_t size) {
  return memcpy(dest, src, size);
}
