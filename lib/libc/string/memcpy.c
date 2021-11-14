#include <string.h>

void* memcpy(void *dest, const void *src, size_t num) {
  register char *_dest = (char*)dest, *_src = (char*)src;

  if(src != NULL && dest != NULL) {
    while(num--)
      *(_dest++) = *(_src++);
  }

  return dest;
}

