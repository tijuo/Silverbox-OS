#include <string.h>

char *strrchr(const char *s, int c)
{
  char *addr = NULL;

  if(s == NULL)
    return NULL;

  while( *s != '\0' )
  {
    if( *s == (char)c )
      addr = (char *)s;
    s++;
  }

  return addr;
}

