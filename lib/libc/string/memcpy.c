#include <string.h>

void* memcpy(void *dest, const void *src, size_t num) {
  char *_dest = (char*)dest;
  const char *_src = (const char*)src;

  if(src != NULL && dest != NULL) {
    while(num--) {
      *_dest = *_src;
      _dest++;
      _src++;
    }
  }

  return dest;
}

