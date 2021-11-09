#include <stdio.h>
#include <errno.h>

int _flushToFile(FILE *stream);

int fflush(FILE *stream)
{
  // If stream is null, then flush all open files

  if( stream == NULL )
  {
    for(size_t i=0; i < FOPEN_MAX; i++)
    {
      if(_stdio_open_files[i].is_open)
      {
        if(fflush(&_stdio_open_files[i]) == EOF)
          return EOF;
      }
    }

    return 0;
  }
  else if(!stream->is_open)
  {
    errno = -ESTALE;
    return EOF;
  }

  // otherwise, flush a particular file

  if(stream->buffer)
  {
    if(_flushToFile(stream) != 0)
    {
      stream->error = 1;
      errno = -EIO;
      return EOF;
    }
  }

  stream->buf_head = 0;
  stream->buf_tail = 0;
  stream->is_buf_full = 0;
  stream->write_req_pos = 0;

  return 0;
}
