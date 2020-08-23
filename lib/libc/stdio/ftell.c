#include <stdio.h>
#include <errno.h>

long ftell(FILE *stream)
{
  if( stream == NULL )
  {
    errno = -EBADF;
    return -1;
  }
  else
    return stream->file_pos;
}
