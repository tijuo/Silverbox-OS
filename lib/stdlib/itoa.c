#include <stdlib.h>

char *_itoa(int value, char *str, int base)
{
  char _buf[33], *buf=_buf;
  unsigned digit;
  char *begin = str;
  unsigned *_val = (unsigned *)&value;

  if( str == NULL || base > 36 || base < 2 )
    return NULL;
  else if( value == 0 )
  {
    *str = '0';
    *(str+1) = '\0';
    return str;
  }

  *buf++ = '\0';

  if( base == 10 && value < 0)
    *str++ = '-';
  
  while( (base == 10 ? value: *_val) )
  {
    digit = (base == 10 ? abs(value % base) : *_val % base);
    *buf++ = (digit >= 10 ? (digit - 10) + 'a' : digit + '0');

    if( base == 10 )
      value /= base;
    else
      *_val /= base;
  }

  while( *--buf != '\0' )
    *str++ = *buf;

  *str = '\0';
  return begin;
}
