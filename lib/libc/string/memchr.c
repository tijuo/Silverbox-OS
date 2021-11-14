#include <string.h>
#pragma GCC diagnostic ignored "-Wdiscarded-qualifiers"

void* memchr(const void *buffer, int c, size_t num) {
  const unsigned char *buff = (const unsigned char*)buffer;

  for(; num && *buff != (unsigned char)c; num--, buff++)
    ;

  return (num ? buff : NULL);
}
