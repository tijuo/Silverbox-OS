#include <stdio.h>
#include <os/console.h>

extern char kbGetChar(void);

int fgetc(FILE *stream)
{
  return (stream == stdin) ? kbGetChar() : EOF;
}
