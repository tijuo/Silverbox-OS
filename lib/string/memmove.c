#include <string.h>

void *memmove(void *dest, const void *src, size_t num)
{
  unsigned i;

  unsigned char *destin = (unsigned char *)dest;
  const unsigned char *source = (unsigned char *)src;

  unsigned char buffer[num]; // Use malloc instead?

  for(i=0; i < num; i++)
    buffer[i] = source[i];

  for(i=0; i < num; i++)
    destin[i] = buffer[i];
  return (void *)destin;
}

