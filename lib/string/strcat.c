#include <string.h>

char *strcat(char *dest, const char *src)
{
  char *start = dest;

  while(*dest++);
  dest--;

  while(*src)
    *dest++ = *src++;

  *dest = '\0';
  return start;
}
