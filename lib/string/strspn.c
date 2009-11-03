#include <string.h>

size_t strspn(const char* cs, const char* ct)
{
  size_t num = 0;
  const char *start = (const char *)ct;

  while(*cs)
  {
    while(*ct)
    {
      if( *cs == *ct )
      {
        num++;
        break;
      }
      else
        ct++;
    }

    if(!*ct)
      return num;
    else
    {
      cs++;
      ct = start;
    }
  }
  return num;
}
