#include <stdio.h>
#include <errno.h>

int fseek(FILE *stream, long offset, int whence)
{
  errno = 0;

  if( stream == NULL )
  {
    errno = -EBADF;
    return -1;
  }
  else if( whence != SEEK_SET && whence != SEEK_END && whence != SEEK_CUR )
  {
    errno = -EINVAL;
    return -1;
  }
  else
  {
    stream->file_pos = (whence == SEEK_SET ? stream->file_pos : 
                       (whence == SEEK_END ? stream->file_len : 0)) + offset;
    return 0;
  }
}
