#include <stdio.h>
#include <errno.h>

int fseek(FILE *stream, long offset, int whence) {
  if(stream == NULL) {
    errno = -EBADF;
    return -1;
  }
  else if(!stream->is_open) {
    errno = -ESTALE;
    return -1;
  }
  else if(whence != SEEK_SET && whence != SEEK_END && whence != SEEK_CUR) {
    errno = -EINVAL;
    return -1;
  }
  else {
    stream->file_pos = (
        whence == SEEK_CUR ? stream->file_pos :
                             (whence == SEEK_END ? stream->file_len : 0))
                       + offset;
    return 0;
  }
}
