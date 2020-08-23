#include <stdio.h>

void clearerr(FILE *stream)
{
  if(stream == NULL)
    return;
  else
  {
    stream->error = 0;
    stream->eof = 0;
  }
}
