#include <stdio.h>
#include <errno.h>
#include <os/console.h>

extern char kbGetChar(void);

int fgetc(FILE *stream)
{
  if(stream)
  {
    if(stream->eof)
      return EOF;
    else
    {
      if(stream->performed_write)
      {
        stream->performed_write = 0;
        // todo: reposition file pos (if applicable)

        if(fflush(stream) == EOF)
          return EOF;
      }

      stream->performed_read = 1;

      return (stream == stdin) ? kbGetChar() : EOF;
    }
  }
  else
  {
    if(!stream->is_open)
      return -ESTALE;

    return EOF;
  }
}
