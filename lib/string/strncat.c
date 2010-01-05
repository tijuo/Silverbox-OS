#include <string.h>

char *strncat(char *dest, const char *src, size_t num)
{
  char *start = dest;

  while(*dest++);
  dest--;

  while(*src && num--)
    *dest++ = *src++;

  if( num )
    *dest = '\0'; 

  return (char *)start;
}

