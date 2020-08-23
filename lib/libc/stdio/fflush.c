#include <stdio.h>
#include <os/console.h>

int fflush(FILE *stream)
{
  if( stream == NULL )
    return -1;

  if( stream == stdout )
  {
//    printStrN(stream->buffer, stream->buf_pos);

    stream->buf_pos = 0;
  }

  return 0;
}
