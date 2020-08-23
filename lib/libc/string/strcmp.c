#include <string.h>

int strcmp(const char *s1, const char *s2)
{
  if(s1 && s2)
  {
    while( ( *s1 ) && ( *s1 == *s2 ) )
    {
      ++s1;
      ++s2;
    }
    return *s1 - *s2;
  }
  else if(!s2)
    return *s1;
  else if(!s1)
    return -(*s2);
  else
    return NULL;
}
