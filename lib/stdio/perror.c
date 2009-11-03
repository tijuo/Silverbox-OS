#include <stdio.h>
#include <errno.h>

void perror(const char *prefix)
{
  fprintf(stderr, "%s: %s\n", prefix, _errno_reasons[errno]);
}
