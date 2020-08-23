#include <stdio.h>
#include <stdlib.h>

int fclose(FILE *stream)
{
  if( !stream )
    return EOF;

  fflush(stream);

  if( !stream->user_buf )
    free(stream->buffer);

  free(stream);

  return 0;
}
