#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

int fclose(FILE *stream) {
  if(!stream) {
    errno = -EINVAL;
    return EOF;
  }
  else if(!stream->is_open) {
    errno = -ESTALE;
    return EOF;
  }

  int ret = fflush(stream);

  if(stream->write_req_buffer)
    free(stream->write_req_buffer);

  stream->is_open = 0;

  return ret;
}
