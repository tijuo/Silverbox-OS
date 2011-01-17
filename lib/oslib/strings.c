#include <oslib.h>
#include <string.h>

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
  unsigned i = 0, j = 0;
  const char hexChars[] = { '0','1','2','3','4','5','6','7','8','9', 'A', 'B',
                         'C', 'D', 'E', 'F' };
  static char s_num[9];
  unsigned h_num[9];

  s_num[0] = 0;

  if(num == 0)
  {
    s_num[0] = '0';
    s_num[1] = '\0';
    return s_num;
  }

  for( ; num > 0; i++, num /= 16 )
    h_num[i] = num % 16;

  i--;

  while( (i + 1) > 0 )
    s_num[j++] = hexChars[h_num[i--]];

  s_num[j] = '\0';

  return s_num;
}

char *toIntString(int num)
{
  unsigned i = 0, j = 0;
  const char iChars[] = { '0','1','2','3','4','5','6','7','8','9' };
  unsigned d_num[17];
  static char s_num[11];

  s_num[0] = 0;

  if(num == 0){
    s_num[0] = '0';
    s_num[1] = '\0';
    return s_num;
  }

  if(num < 0) {
    s_num[j++] = '-';
    num = -num;
  }

  for( ; num > 0; i++, num /= 10 )
    d_num[i] = num % 10;

  i--;

  while((i + 1) > 0)
    s_num[j++] = iChars[d_num[i--]];

  s_num[j] = '\0';
  return s_num;
}
