#include <string.h>

void *memset(void *buffer, int c, register size_t num)
{
  char *buff = (char *)buffer;

  while(num--)
    *(buff++) = (char)c;

  return buffer;
}
