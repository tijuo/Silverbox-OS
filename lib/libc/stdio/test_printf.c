#include <stdio.h>
#include <stdarg.h>

extern int do_printf(void *s, const char *format, va_list ap, int isStream);
int tprintf(const char *format,va_list arglist);

int tprintf(const char *format, va_list arglist)
{
  return do_printf(stdout, format, arglist, 1);
}

void test_printf(const char *format, ...)
{
  int result, result2;

  va_list arglist;

  va_start(arglist, format);
  result = tprintf(format, arglist);
  va_end(arglist);

  va_start(arglist, format);
  result2 = vprintf(format, arglist);
  va_end(arglist);

  printf("tprintf: %4d characters written\n", result);
  printf("printf:  %4d characters written\n\n", result2);
}

int main(void)
{
  test_printf("Hello, %10s!\n", "world");
  test_printf("%d + %d = %d\n", 2,2,4);
  test_printf("%4.8d\n", 22);
  test_printf("%8.4d\n", 22);
  test_printf("%4.8d\n", -22);
  test_printf("%8.4d\n", -22);
  test_printf("%+4.8d\n", 1234567890);
  test_printf("%+8.4d\n", 1234567890);
  test_printf("%0.6x\n", 0xc0ffee);
  test_printf("%0.6X\n", 0xc0ffee);
  test_printf("%u\n", 0xFFFFFFFF);;
  test_printf("%p\n", 0x80000000);

  return 0;
}
