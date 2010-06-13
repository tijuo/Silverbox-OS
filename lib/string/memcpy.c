#include <string.h>

void *memcpy(void *dest, const void *src, size_t num)
{
  char *_dest=dest, *_src=(char *)src;

  if( src == NULL || dest == NULL )
    return NULL;

  while( num )
  {
    *_dest++ = *_src++;
    num--;
  }

  return (void *)dest;
}

