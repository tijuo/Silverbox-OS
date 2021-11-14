#include <ctype.h>
#include <stdlib.h>

#define MIN_BASE        2
#define MAX_BASE        36

enum strtotypeType {
  TYPE_INT, TYPE_UINT, TYPE_LONG, TYPE_ULONG, TYPE_LLONG, TYPE_ULLONG
};

typedef union {
  int iValue;
  unsigned int uiValue;
  long lValue;
  unsigned long ulValue;
  long long llValue;
  unsigned long long ullValue;
} strtol_t;

static strtol_t strtotype(const char *nptr, char **endptr, int base,
                          enum strtotypeType t);

long long atoll(char *nptr) {
  return strtotype(nptr, (char**)NULL, 10, TYPE_LLONG).llValue;
}

long atol(char *nptr) {
  return strtotype(nptr, (char**)NULL, 10, TYPE_LONG).lValue;
}

int atoi(char *nptr) {
  return strtotype(nptr, (char**)NULL, 10, TYPE_INT).iValue;
}

long strtol(const char *nptr, char **endptr, int base) {
  return strtotype(nptr, endptr, base, TYPE_LONG).lValue;
}

unsigned long strtoul(const char *nptr, char **endptr, int base) {
  return strtotype(nptr, endptr, base, TYPE_ULONG).ulValue;
}

long long strtoll(const char *nptr, char **endptr, int base) {
  return strtotype(nptr, endptr, base, TYPE_LLONG).llValue;
}

unsigned long long strtoull(const char *nptr, char **endptr, int base) {
  return strtotype(nptr, endptr, base, TYPE_ULLONG).ullValue;
}

strtol_t strtotype(const char *nptr, char **endptr, int base,
                   enum strtotypeType t)
{
  int isNeg = 0;
  strtol_t num;

  if(!(base == 0 || (base >= MIN_BASE && base <= MAX_BASE)))
    goto error;

  switch(t) {
    case TYPE_UINT:
      num.uiValue = 0;
      break;
    case TYPE_LONG:
      num.lValue = 0;
      break;
    case TYPE_ULONG:
      num.ulValue = 0;
      break;
    case TYPE_LLONG:
      num.llValue = 0;
      break;
    case TYPE_ULLONG:
      num.ullValue = 0;
      break;
    case TYPE_INT:
    default:
      num.iValue = 0;
      break;
  }

  while(isspace(*nptr))
    nptr++;

  switch(*nptr) {
    case '-':
      isNeg = 1;
      nptr++;
      break;
    case '+':
      nptr++;
      break;
    default:
      break;
  }

  if(base == 0) {
    if(*nptr == '0') {
      nptr++;

      if(tolower(*nptr) == 'x') {
        base = 16;
        nptr++;
      }
      else
        base = 8;
    }
    else
      base = 10;
  }

  while(*nptr) {
    if(!isalnum(*nptr))
      goto error;

    int digit = isdigit(*nptr) ? *nptr - '0' : tolower(*nptr) - 'a' + 10;

    if(digit >= base)
      goto error;

    switch(t) {
      case TYPE_UINT:
        num.uiValue = num.uiValue * base + (unsigned int)digit;
        break;
      case TYPE_LONG:
        num.lValue = num.lValue * base + (long)digit;
        break;
      case TYPE_ULONG:
        num.ulValue = num.ulValue * base + (unsigned long)digit;
        break;
      case TYPE_LLONG:
        num.llValue = num.llValue * base + (long long)digit;
        break;
      case TYPE_ULLONG:
        num.ullValue = num.ullValue * base + (unsigned long long)digit;
        break;
      case TYPE_INT:
      default:
        num.iValue = num.iValue * base + digit;
        break;
    }

    nptr++;
  }

  return num;
error:
  if(endptr)
#pragma GCC diagnostic ignored "-Wdiscarded-qualifiers"
    *endptr = nptr;

  switch(t) {
    case TYPE_UINT:
      num.uiValue = isNeg ? -num.uiValue : num.uiValue;
      break;
    case TYPE_LONG:
      num.lValue = isNeg ? -num.lValue : num.lValue;
      break;
    case TYPE_ULONG:
      num.ulValue = isNeg ? -num.ulValue : num.ulValue;
      break;
    case TYPE_LLONG:
      num.llValue = isNeg ? -num.llValue : num.llValue;
      break;
    case TYPE_ULLONG:
      num.ullValue = isNeg ? -num.ullValue : num.ullValue;
      break;
    case TYPE_INT:
    default:
      num.iValue = isNeg ? -num.iValue : num.iValue;
      break;
  }

  return num;
}
