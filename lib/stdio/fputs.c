#include <stdio.h>
#include <string.h>

int fputs(const char *s, FILE *stream)
{
  size_t len = strlen(s);

  while(len)
  {
    if( fputc(*s++, stream) == EOF )
      return EOF;
    len--;
  }

  return 0;
}
