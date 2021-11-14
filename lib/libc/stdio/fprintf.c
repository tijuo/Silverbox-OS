#include <stdio.h>
#include <stdarg.h>

int fprintf(FILE *stream, const char *format, ...) {
  va_list args;
  int retval;

  va_start(args, format);
  retval = vfprintf(stream, format, args);
  va_end(args);

  return retval;
}
