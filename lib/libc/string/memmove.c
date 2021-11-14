#include <string.h>
#include <stdlib.h>

void* memmove(void *dest, const void *src, size_t num) {
  unsigned char *destin = (unsigned char*)dest;
  const unsigned char *source = (unsigned char*)src;
  unsigned char *buffer = malloc(num); // Use malloc instead?

  if(!buffer)
    return NULL;

  memcpy(buffer, source, num);
  memcpy(destin, buffer, num);

  free(buffer);
  return destin;
}

