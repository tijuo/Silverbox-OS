#include <string.h>

size_t strlen(const char *string)
{
  const char *start=string;

  if( string )
  {
    while( *string )
      string++;
  }

  return string-start;
}
