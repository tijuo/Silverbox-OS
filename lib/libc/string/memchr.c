#include <string.h>

void* memchr(const void *buffer, int c, size_t num) {
  size_t i;
  unsigned char *buff = (unsigned char*)buffer;

  for(i = 0; i < num && *buff != c; i++, buff++)
    ;

  return (i == num ? NULL : buff);
}
