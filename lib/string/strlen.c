#include <string.h>

size_t strlen(const char *string)
{
  register size_t i=0;

  if( string == NULL )
    return 0;

  while( *string++ != '\0' )
    i++;

  return i;
}
