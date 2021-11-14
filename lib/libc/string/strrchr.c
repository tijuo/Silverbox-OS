#include <string.h>

char* strrchr(const char *s, int c) {
  char *addr = NULL;

  if(s) {
    while(*s) {
      if(*s == c)
        addr = (char*)s;
      s++;
    }
  }

  return addr;
}

