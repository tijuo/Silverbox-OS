#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//#include <os/dev_interface.h>
// #include <os/services.h>
#include <os/console.h>

extern int printChar(char);

int fputc(int c, FILE *stream)
{
  if( stream == NULL )
    return EOF;

  if( stream->buf_mode != _IONBF && stream->buffer == NULL )
  {
    stream->buffer = malloc(stream->buffer_len);
    stream->user_buf = 0;
    stream->buf_pos = 0;
    stream->buffer_len = BUFSIZ;

    if( stream->buffer == NULL )
      return EOF;
  }

  if( stream->buf_mode != _IONBF )
  {
    if( stream->buf_pos == stream->buffer_len )
    {
      fflush(stream);

      if( stream->buf_pos == stream->buffer_len )
        return EOF;
    }

    stream->buffer[stream->buf_pos++] = (char)c;

    if( stream->buf_mode == _IOLBF && (char)c == '\n' )
      fflush(stream);

    return c;
  }
  else
    ; // put the raw char
/*
  if( stream == stdout || stream == stderr )
  {
    

    if( stream->buf_mode == _IOLBF )
    {
      
    }

    if( printChar((char)c) < 0 )
      return EOF;
    else
      return (int)c;
  }
*/
  return EOF;
}
