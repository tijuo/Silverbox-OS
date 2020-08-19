#include <string.h>

char *strchr(const char *s, int c)
{
  if(s != NULL)
  {
    for( ; *s && *s != c; s++ );

    if(*s == c)
      return (char *)s;
  }

  return NULL;
}
