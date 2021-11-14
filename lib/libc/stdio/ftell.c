#include <stdio.h>
#include <errno.h>
#include <limits.h>

long ftell(FILE *stream) {
  if(stream == NULL) {
    errno = -EBADF;
    return -1;
  }
  else if(!stream->is_open) {
    errno = -ESTALE;
    return -1;
  }
  else {
    if(stream->file_pos > ULONG_MAX) {
      errno = -EOVERFLOW;
      return -1;
    }
    else
      return (long)stream->file_pos;
  }
}
