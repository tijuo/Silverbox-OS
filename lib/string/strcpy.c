#include <string.h>

char *strcpy(char *s1, const char *s2)
{
  char *rc = s1;
  while((*(s1++) = *(s2++)));
  return rc;
}
