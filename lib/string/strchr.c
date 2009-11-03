#include <string.h>

char *strchr(const char *s, int c)
{
  if(s == NULL)
    return NULL;

  while( *s )
  {
    if( *s++ == (char)c )
      return (char *)(s-1);
  }

  return NULL;
}
