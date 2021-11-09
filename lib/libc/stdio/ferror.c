#include <stdio.h>
#include <errno.h>

int ferror(FILE *stream)
{
  if(!stream)
  {
    errno = -EINVAL;
    return 1;
  }
  else if(!stream->is_open)
  {
    errno = -ESTALE;
    return 1;
  }
  else
    return stream->error;
}
