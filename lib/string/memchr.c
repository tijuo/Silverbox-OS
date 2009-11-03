#include <string.h>

void *memchr(const void *buffer, int c, size_t num)
{
  unsigned i;
  unsigned char *buff = (unsigned char *)buffer;

  for(i=0; (i < num) && (buff[i] != c); i++);

  if(buff[i] != c)
    return NULL;
  else
    return (void *)buff;
}
