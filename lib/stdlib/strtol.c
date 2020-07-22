#include <ctype.h>
#include <stddef.h>
#include <stdlib.h>

enum __strtotype_type { __LONG_TYPE,  __ULONG_TYPE };

long __strtotype(const char *nptr, char **endptr, int base, .../*int type*/);


long atol(char *nptr)
{
  return strtol(nptr, (char **)NULL, 10);
}

int atoi(char *nptr)
{
  return (int)strtol(nptr, (char **)NULL, 10);
}

long strtol(const char *nptr, char **endptr, int base)
{
  return __strtotype(nptr, endptr, base, __LONG_TYPE);
}

unsigned long strtoul(const char *nptr, char **endptr, int base)
{
  return (unsigned long)__strtotype(nptr, endptr, base, __ULONG_TYPE);
}

long __strtotype(const char *nptr, char **endptr, int base, .../*int type*/)
{
  int neg = 0;
  long num=0;

  while(isspace(*nptr))
    nptr++;

  if(*nptr == '-' || *nptr == '+')
    neg = (*nptr++ == '-' ? 1 : 0);

  if(base == 0)
  {
    if(*nptr == '0')
    {
      nptr++;
      if(tolower(*nptr) == 'x')
      {
        base = 16;
        nptr++;
      }
      else
        base = 8;
    }
    else
      base = 10;
  }

  while(*nptr)
  {
    if(!isalnum(*nptr))
      goto error;
    else if( base >= 2 && base <= 10 && *nptr - '0' >= base )
      goto error;
    else if( base > 10 && base < 36 && tolower(*nptr) - 'a' >= base - 10 )
      goto error;

    num *= base;

    if( isdigit(*nptr) )
      num += (neg ? -1 * (*nptr - '0') : (*nptr - '0'));
    else
      num += (neg ? -1 * (10+tolower(*nptr) - 'a') : (10+tolower(*nptr) - 'a'));

    nptr++;
  }

error:
  if(endptr)
    *endptr = (char *)nptr;
  return num;
}
