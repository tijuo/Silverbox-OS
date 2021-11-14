#include <string.h>
#include <stdlib.h>

void* memmove(void *dest, const void *src, size_t num) {
  unsigned char *destin = (unsigned char*)dest;
  const unsigned char *source = (const unsigned char*)src;
  unsigned char *buffer = malloc(num);

  if(!buffer)
    return NULL;

  memcpy(buffer, source, num);
  memcpy(destin, buffer, num);

  free(buffer);
  return destin;
}

