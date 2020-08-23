#include <stdio.h>

char *gets(char *s)
{
  char c;

  while(1)
  {
    c = getchar();

    if( c == EOF || c == '\n' )
    {
      s = '\0';
      break;
    }
    else
      *s++ = c;
  }

  return s;
}
