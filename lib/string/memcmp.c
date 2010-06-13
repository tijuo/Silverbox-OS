#include <string.h>

int memcmp(const void *s1, const void *s2, size_t n)
{
  register const char *str1, *str2;

  str1 = (const char *)s1;
  str2 = (const char *)s2;

  if( n == 0 || (s1 == NULL && s2 == NULL) )
    return 0;

  if( !s1 )
    return -*str2;
  else if( !s2 )
    return *str1;

  if( s1 == NULL || s2 == NULL )
    return -1;

  for(; n && *str1 == *str2; str1++, str2++, n--);

  if( !n )
    return 0;
  else
    return (*str1 - *str2);
}
