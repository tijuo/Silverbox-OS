#include <stdio.h>

int ferror(FILE *stream)
{
  if( stream == NULL )
    return 0;
  else
    return (stream->error == 1);
}
