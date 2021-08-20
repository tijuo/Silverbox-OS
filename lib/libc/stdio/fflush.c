#include <stdio.h>
#include <os/console.h>

int fflush(FILE *stream)
{
  if( stream == NULL )
    return -1;

  if( stream == stdout || stream == stderr )
  {
    for(size_t i=0; i < stream->buf_pos; i++)
      asm("out %%al, %%dx\n" :: "d"(0xE9), "a"((unsigned char)stream->buffer[i]));

    stream->buf_pos = 0;
  }

  return 0;
}
