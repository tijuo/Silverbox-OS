#include <stdio.h>
#include <stdarg.h>

int printf(const char *format, ...) {
  va_list args;
  int retval;

  va_start(args, format);
  retval = vprintf(format, args);
  va_end(args);

  return retval;
}
