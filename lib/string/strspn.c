#include <string.h>

size_t strspn(const char *s, const char *accept)
{
  size_t num = 0;
  const char *start = (const char *)accept;

  if(!accept)
    return strlen(s);

  while(*s)
  {
    while(*accept)
    {
      if(*s == *accept)
      {
        num++;
        break;
      }
      else
        accept++;
    }

    if(!*accept)
      return num;
    else
    {
      s++;
      accept = start;
    }
  }

  return num;
}
