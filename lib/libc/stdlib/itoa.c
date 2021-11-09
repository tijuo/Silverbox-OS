#include <stdlib.h>
#include <limits.h>
#include <string.h>

static char *_digits = "0123456789abcdefghijklmnopqrstuvwxyz";

#define INT_MIN_STR     "-2147483648"

#if __LONG_LEN__ == 8
#define LONG_MIN_STR    "-9223372036854775808"
#else
#define LONG_MIN_STR    INT_MIN_STR
#endif /* __LONG_LEN__ == 8 */

#define LLONG_MIN_STR   "-9223372036854775808"

char *itoa(int value, char * str, int base)
{
  int end = itostr(value, str, base);

  if(end < 0)
    return NULL;
  else
  {
    str[end] = '\0';
    return str;
  }
}

int _itostr(int value, char *str, int base)
{
  size_t strEnd=0;
  int negative = base == 10 && value < 0;
  int charsWritten=0;

  if( !str || base < 2 || base > 36)
    return -1;

  if(negative)
  {
    if(value == INT_MIN)
    {
      memcpy(str, INT_MIN_STR, sizeof INT_MIN_STR - 1);
      return sizeof INT_MIN_STR - 1;
    }
    else
      value = ~value + 1;
  }

  do
  {
    unsigned int quot = base * ((unsigned int)value / base);

    str[strEnd++] = _digits[value - quot];
    value = quot / base;
  } while( value );

  if(negative)
    str[strEnd++] = '-';

  charsWritten = strEnd;

  if( strEnd )
  {
    strEnd--;

    for( size_t i=0; i < strEnd; i++, strEnd-- )
    {
      char tmp = str[i];
      str[i] = str[strEnd];
      str[strEnd] = tmp;
    }
  }

  return charsWritten;
}

int _uitostr(unsigned int value, char *str, int base)
{
  size_t strEnd=0;
  int charsWritten=0;

  if( !str || base < 2 || base > 36)
    return -1;

  do
  {
    unsigned int quot = base * ((unsigned int)value / base);

    str[strEnd++] = _digits[value - quot];
    value = quot / base;
  } while( value );

  charsWritten = strEnd;

  if( strEnd )
  {
    strEnd--;

    for( size_t i=0; i < strEnd; i++, strEnd-- )
    {
      char tmp = str[i];
      str[i] = str[strEnd];
      str[strEnd] = tmp;
    }
  }

  return charsWritten;
}

int _litostr(long int value, char *str, int base)
{
  size_t strEnd=0;
  int negative = base == 10 && value < 0;
  int charsWritten=0;

  if( !str || base < 2 || base > 36)
    return -1;

  if(negative)
  {
    if(value == (long int)LONG_MIN)
    {
      memcpy(str, LONG_MIN_STR, sizeof LONG_MIN_STR - 1);
      return sizeof LONG_MIN_STR - 1;
    }
    else
      value = ~value + 1;
  }

  do
  {
    long int quot = base * (value / base);

    str[strEnd++] = _digits[value - quot];
    value = quot / base;
  } while( value );

  if(negative)
    str[strEnd++] = '-';

  charsWritten = strEnd;

  if( strEnd )
  {
    strEnd--;

    for( size_t i=0; i < strEnd; i++, strEnd-- )
    {
      char tmp = str[i];
      str[i] = str[strEnd];
      str[strEnd] = tmp;
    }
  }

  return charsWritten;
}

int _ulitostr(unsigned long int value, char *str, int base)
{
  size_t strEnd=0;
  int charsWritten=0;

  if( !str || base < 2 || base > 36)
    return -1;

  do
  {
    unsigned long int quot = base * ((unsigned long int)value / base);

    str[strEnd++] = _digits[value - quot];
    value = quot / base;
  } while( value );

  charsWritten = strEnd;

  if( strEnd )
  {
    strEnd--;

    for( size_t i=0; i < strEnd; i++, strEnd-- )
    {
      char tmp = str[i];
      str[i] = str[strEnd];
      str[strEnd] = tmp;
    }
  }

  return charsWritten;
}

int _llitostr(long long int value, char *str, int base)
{
  size_t strEnd=0;
  int negative = base == 10 && value < 0;
  int charsWritten=0;
  unsigned int v[2] = { (unsigned int)(value & 0xFFFFFFFFu), (unsigned int)(value >> 32) };

  if( !str || base < 2 || base > 36)
    return -1;

  if(negative)
  {
    if(value == (long long int)LLONG_MIN)
    {
      memcpy(str, LLONG_MIN_STR, sizeof LLONG_MIN_STR - 1);
      return sizeof LLONG_MIN_STR - 1;
    }
    else
      value = ~value + 1;
  }

  for(int i=0; i < 2; i++)
  {
    if(i == 1 && v[i] == 0)
      break;

    do
    {
      unsigned int quot = base * ((unsigned int)v[i] / base);

      str[strEnd++] = _digits[v[i] - quot];
      v[i] = quot / base;
    } while( v[i] );
  }

  if(negative)
    str[strEnd++] = '-';

  charsWritten = strEnd;

  if( strEnd )
  {
    strEnd--;

    for( size_t i=0; i < strEnd; i++, strEnd-- )
    {
      char tmp = str[i];
      str[i] = str[strEnd];
      str[strEnd] = tmp;
    }
  }

  return charsWritten;
}

int _ullitostr(unsigned long long int value, char *str, int base)
{
  size_t strEnd=0;
  int charsWritten=0;
  unsigned int v[2] = { (unsigned int)(value & 0xFFFFFFFFu), (unsigned int)(value >> 32) };

  if( !str || base < 2 || base > 36)
    return -1;

  for(int i=0; i < 2; i++)
  {
    if(i == 1 && v[i] == 0)
      break;

    do
    {
      unsigned int quot = base * ((unsigned int)v[i] / base);

      str[strEnd++] = _digits[v[i] - quot];
      v[i] = quot / base;
    } while( v[i] );
  }

  charsWritten = strEnd;

  if( strEnd )
  {
    strEnd--;

    for( size_t i=0; i < strEnd; i++, strEnd-- )
    {
      char tmp = str[i];
      str[i] = str[strEnd];
      str[strEnd] = tmp;
    }
  }

  return charsWritten;
}
