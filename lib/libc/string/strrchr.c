#include <string.h>
#pragma GCC diagnostic ignored "-Wcast-qual"

char* strrchr(const char *s, int c) {
  const char *addr = NULL;

  if(s) {
    while(*s) {
      if(*s == c)
        addr = (const char*)s;
      s++;
    }
  }

  return (char*)addr;
}

