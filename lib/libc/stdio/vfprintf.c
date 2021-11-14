#include <stdio.h>
#include <stdarg.h>

extern int do_printf(void *s, int (*writeFunc)(int, void**),
                     const char *formatStr, va_list args);

static int writeToStream(int c, void **stream) {
  return (fputc(c, *(FILE**)stream) == EOF ? -1 : 0);
}

int vfprintf(FILE *stream, const char *format, va_list ap) {
  return do_printf(&stream, &writeToStream, format, ap);
}
