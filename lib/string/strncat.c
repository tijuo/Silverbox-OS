#include <string.h>

char *strncat(char *dest, const char *src, size_t num)
{
  char *start = dest;

  dest += strlen(dest);

  while(*src && num)
  {
    *(dest++) = *(src++);
    num--;
  }

  *dest = '\0';

  return (char *)start;
}

