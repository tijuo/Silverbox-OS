#include <stdio.h>
#include <errno.h>

void clearerr(FILE *stream) {
  if(stream) {
    stream->error = 0;
    stream->eof = 0;
  }
  else
    errno = -EINVAL;
}
