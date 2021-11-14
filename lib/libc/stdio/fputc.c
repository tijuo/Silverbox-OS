#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

int _writeCharToFile(FILE *stream, int c);

int fputc(int c, FILE *stream) {
  if(stream == NULL) {
    errno = -EINVAL;
    return EOF;
  }
  else if(!stream->is_open) {
    errno = -ESTALE;
    return EOF;
  }
  else if(stream->eof) {
    errno = -EIO;
    return EOF;
  }

  if(stream->performed_read) {
    stream->performed_read = 0;
    // todo: reposition file pos (if applicable)

  }

  if(stream->buf_mode != _IONBF) {
    if(stream->is_buf_full && fflush(stream) == EOF)
      return EOF;

    stream->buffer[stream->buf_head++] = (char)c;

    if(stream->buf_head == stream->buffer_len)
      stream->buf_head = 0;

    if(stream->buf_head == stream->buf_tail)
      stream->is_buf_full = 1;

    if(stream->buf_mode == _IOLBF && (char)c == '\n' && fflush(stream) == EOF)
      return EOF;
    else
      return c;
  }
  else if(_writeCharToFile(stream, c) == 0)
    return c;

  errno = -EIO;
  stream->error = 1;
  return EOF;
}
