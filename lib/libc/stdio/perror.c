#include <stdio.h>
#include <errno.h>

void perror(const char *prefix) {
  if(!prefix || *prefix == '\0') {
    if(errno != 0)
      fprintf(stderr, "%s\n", _errno_reasons[errno]);
  }
  else if(errno == 0)
    fprintf(stderr, "%s", prefix);
  else
    fprintf(stderr, "%s: %s\n", prefix, _errno_reasons[errno]);
}
