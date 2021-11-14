#include <string.h>

int memcmp(const void *s1, const void *s2, size_t n) {
  register const unsigned char *str1 = (const unsigned char*)s1;
  register const unsigned char *str2 = (const unsigned char*)s2;

  if(n == 0 || (s1 == NULL && s2 == NULL))
    return 0;

  if(!s1)
    return -*str2;
  else if(!s2)
    return *str1;

  for(; n && *str1 == *str2; str1++, str2++, n--)
    ;

  return (!n ? 0 : (*str1 - *str2));
}
