#include <stdio.h>
#include <stdarg.h>

int sprintf(char *str, const char *format, ...) {
  va_list args;
  int retval;

  va_start(args, format);
  retval = vsprintf(str, format, args);
  va_end(args);

  return retval;
}
