#include <string.h>

char *strstr(const char* cs, const char* ct) 
{
  const char *start = (const char *)ct;

  while(*cs)
  {
    while(*ct)
    {
      if( *cs == *ct )
        return (char *)cs;
      else
        ct++;
    }

    cs++;
    ct = start;
  }
  return NULL;
}

