#include <stdio.h>
#include <errno.h>

int feof(FILE *fp) {
  if(!fp) {
    errno = -EINVAL;
    return 1;
  }
  else if(!fp->is_open) {
    errno = -ESTALE;
    return 1;
  }
  else
    return fp->eof;
}
