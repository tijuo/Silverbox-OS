#include <stdio.h>
#include <stdarg.h>

extern int do_printf(void **s, int (*writeFunc)(int, void **), const char *formatStr, va_list args);

static int writeToStr(int c, void **s)
{
  if(!(char **)*s)
    return -1;
  else
  {
    **(char **)s = (char)c;
    *(char **)s = *(char **)s + 1;
    return 0;
  }
}

int vsprintf(char *str, const char *format, va_list ap)
{
  int retval = do_printf((void **)&str, writeToStr, format, ap);
  writeToStr('\0', (void **)&str);
  return retval;
}
