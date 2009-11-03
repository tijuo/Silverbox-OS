#include <string.h>

char *strrchr(const char *s, int c)
{
  char *addr = NULL;

  if(s == NULL)
    return NULL;

  while( *s )
  {
    if( *s++ == (char)c )
      addr = (char *)(s-1);
  }

  return addr;
}

