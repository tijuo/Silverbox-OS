#include <ctype.h>
#include <stddef.h>
#include <stdlib.h>

enum _strtotype_type { __LONG_TYPE,  __ULONG_TYPE };

long _strtotype(const char *nptr, char **endptr, int base, .../*int type*/);

#define MIN_BASE        2
#define DEC_BASE        10
#define OCT_BASE         8
#define HEX_BASE        16
#define MAX_BASE        36

long atol(char *nptr)
{
  return strtol(nptr, (char **)NULL, DEC_BASE);
}

int atoi(char *nptr)
{
  return (int)strtol(nptr, (char **)NULL, DEC_BASE);
}

long strtol(const char *nptr, char **endptr, int base)
{
  return _strtotype(nptr, endptr, base, __LONG_TYPE);
}

unsigned long strtoul(const char *nptr, char **endptr, int base)
{
  return (unsigned long)_strtotype(nptr, endptr, base, __ULONG_TYPE);
}

long _strtotype(const char *nptr, char **endptr, int base, .../*int type*/)
{
  int neg = 0;
  long num=0;

  while(isspace(*nptr))
    nptr++;

  if(*nptr == '-' || *nptr == '+')
    neg = (*(nptr++) == '-');

  if(base == 0)
  {
    if(*nptr == '0')
    {
      if(tolower(*(++nptr)) == 'x')
      {
        base = HEX_BASE;
        nptr++;
      }
      else
        base = OCT_BASE;
    }
    else
      base = DEC_BASE;
  }

  while(*nptr)
  {
    if(!isalnum(*nptr))
      goto error;
    else if( base >= MIN_BASE && base <= DEC_BASE && *nptr - '0' >= base )
      goto error;
    else if( base > DEC_BASE && base < MAX_BASE && tolower(*nptr) - 'a' >= base - DEC_BASE )
      goto error;

    num = num * base + (neg ? -1 : 1) * (isdigit(*nptr) ? (*nptr - '0') : (DEC_BASE + tolower(*nptr) - 'a'));
    nptr++;
  }

error:
  if(endptr)
    *endptr = (char *)nptr;
  return num;
}
