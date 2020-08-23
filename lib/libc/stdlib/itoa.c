#include <stdlib.h>
#include <limits.h>

static const char *digits = "0123456789abcdefghijklmnopqrstuvwxyz";

char *itoa(int v, char *str, int base)
{
  char *ptr = str;
  char *retStr = str;
  int neg = (base == 10 && v < 0);
  unsigned int value;

  if(!str)
    return retStr;

  *(ptr++) = '\0';

  if(base < 0)
    base = -base;

  if(base < 2 || base > 36)
    return retStr;

  if(neg)
    v = -v;

  value = (unsigned int)v;

  do
  {
    *(ptr++) = digits[value % base];
    value /= base;
  } while(value);

  if(neg)
    *(ptr++) = '-';

  for(; --ptr > str; str++)
  {
    char t = *ptr;
    *ptr = *str;
    *str = t;
  }

  return retStr;
}
