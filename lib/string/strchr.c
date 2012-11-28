#include <string.h>

char *strchr(const char *s, int c)
{
  if(s == NULL)
    return NULL;

  for( ; *s && *s != c; s++ );

  if( *s == c )
    return (char *)s;
  else
    return NULL;
}
