#include <stdio.h>
#include <string.h>

int fread(void *ptr, size_t size, size_t count, FILE *stream)
{
  int bytes_read = 0;

  if( !stream || !ptr )
    return EOF;

  if( feof(stream) )
    return EOF;

  if( !stream->buffer )
  {
    stream->error = 1;
    return EOF;
  }

  if( !(stream->access & ACCESS_RD) )
  {
    stream->error = 1;
    return EOF;
  }

  if( stream->buf_pos + size * count > stream->buffer_len )
  {
    memcpy(ptr, &stream->buffer[stream->buf_pos],
      stream->buffer_len - stream->buf_pos);
    bytes_read += (stream->buffer_len - stream->buf_pos);
    stream->file_pos += (stream->buffer_len - stream->buf_pos);
    stream->buf_pos = stream->buffer_len;
  }
  else
  {
    memcpy(ptr, &stream->buffer[stream->buf_pos], size * count);
    bytes_read += size * count;
    stream->file_pos += size * count;
    stream->buf_pos += size * count;
  }

  /* XXX: Read more data from the file if there is more to be read,
     but the buffer has been completely read. */

  if( stream->file_pos == stream->file_len )
    stream->eof = 1;

  return bytes_read;
}
