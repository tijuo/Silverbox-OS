#include <stdio.h>
#include <stdarg.h>

extern int do_printf(void *s, const char *format, va_list ap, int is_stream);

int vsprintf(char *str, const char *format, va_list ap)
{
  return do_printf(str, format, ap, 0);
}
