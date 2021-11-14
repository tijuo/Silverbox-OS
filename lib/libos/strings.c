#include <oslib.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

char* strdup(const char *str) {
  char *newStr = malloc(strlen(str) + 1);

  if(newStr)
    strcpy(newStr, str);
  return newStr;
}

char* strndup(const char *str, size_t n) {
  char *newStr = calloc(n, 1);

  strncpy(newStr, str, n);
  return newStr;
}

char* strappend(const char *str, const char *add) {
  char *newStr = NULL;
  size_t s1;
  size_t s2;

  if(!str)
    return strdup(add);
  else if(!add)
    return strdup(str);

  s1 = strlen(add);
  s2 = strlen(str);

  newStr = malloc(s1 + s2 + 1);
  strcpy(newStr, str);
  strcat(newStr, add);

  return newStr;
}
