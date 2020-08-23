#include <oslib.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

char *strdup(const char *str)
{
  char *newStr = malloc(strlen(str)+1);

  if( newStr )
    strcpy(newStr, str);
  return newStr;
}

char *strndup(const char *str, size_t n)
{
  char *newStr = calloc(n, 1);

  strncpy(newStr, str, n);
  return newStr;
}

char *strappend(const char *str, const char *add)
{
  char *newStr=NULL;
  size_t s1,s2;

  if( !str )
    return strdup(add);
  else if( !add )
    return strdup(str);

  s1=strlen(add);
  s2=strlen(str);

  newStr = malloc(s1+s2+1);
  strcpy(newStr, str);
  strcat(newStr, add);

  return newStr;
}

/*
char *toOctalString( unsigned int num )
{
  unsigned i = 0;
  static char s_num[13];

  do
  {
    s_num[i++] = (num >> 29) + '0';
    num <<= 3;
  } while( num != 0 );

  s_num[i] = '\0';
  return s_num;
}
*/

char *toHexString(unsigned int num)
{
  static char str[9];

  if(sprintf(str, "%X", num) < 0)
    str[0] = '\0';

  return str;
}

char *toIntString(int num)
{
  static char str[17];

  if(sprintf(str, "%d", num) < 0)
    str[0] = '\0';

  return str;
}
