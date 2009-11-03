#include <stdio.h>
#include <os/console.h>

extern char kbGetChar(void);

int fgetc(FILE *stream)
{
  if( stream == stdin )
    return (int)kbGetChar();
  else
    return EOF;
}
