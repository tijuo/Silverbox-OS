#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

int setvbuf(FILE *stream, char *buf, int mode, size_t size)
{
  errno = 0;

  if(stream == NULL)
  {
    errno = -EBADF;
    return -1;
  }
  else
  {
    if( mode != _IOFBF && mode != _IONBF && mode != _IOLBF )
    {
      errno = EINVAL;
      return -1;
    }

    /* If buf is NULL and the mode is either _IOFBF or _IOLBF,
       a new buffer will be allocated on the next read/write */

    if( !stream->user_buf && stream->buffer != NULL )
      free( stream->buffer );

    stream->user_buf = (buf == NULL ? 0 : 1);
    stream->buffer = buf;
    stream->buf_mode = mode;
    stream->buffer_len = size;
    return 0;
  }
}
