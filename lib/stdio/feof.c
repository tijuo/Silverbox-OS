#include <stdio.h>

int feof(FILE *fp)
{
  return (!fp) ? 0 : (fp->eof);
}
