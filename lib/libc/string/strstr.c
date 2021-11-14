#include <string.h>

char* strstr(const char *haystack, const char *needle) {
  const char *needleStart = (const char*)needle;
  const char *haystackStart = haystack;

  if(haystack) {
    while(*haystack) {
      while(*needle) {
        if(*haystack == *needle) {
          haystack++;
          needle++;
        }
        else
          break;
      }

      if(!*needle)
        return (char*)haystackStart;
      else {
        haystack = haystackStart + 1;
        needle = needleStart;
      }
    }
  }

  return NULL;
}

