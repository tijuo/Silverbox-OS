#ifndef SBSTRING_H
#define SBSTRING_H

#include <stdlib.h>
#include <os/ostypes/sbarray.h>
#include <os/ostypes/ostypes.h>

typedef struct sbstring_t
{
  char *data;
  size_t length;
  size_t charWidth;
} sbstring_t;

char sbStringCharAt(const sbstring_t *str, size_t index);
size_t sbStringCharWidth(const sbstring_t *str);
int sbStringCompare(const sbstring_t *str1, const sbstring_t *str2);
sbstring_t *sbStringConcat(sbstring_t *str, const sbstring_t *addend);
sbstring_t *sbStringCopy(const sbstring_t *str, sbstring_t *newStr);
sbstring_t *sbStringCreate(sbstring_t *sbString, const char *str);
sbstring_t *sbStringCreateN(sbstring_t *sbString, const char *str, size_t len);
int sbStringDelete(sbstring_t *sbString);
int sbStringFind(const sbstring_t *haystack, const sbstring_t *needle);
int sbStringFindChar(const sbstring_t *sbString, int c);
size_t sbStringLength(const sbstring_t *str);
sbarray_t *sbStringSplit(const sbstring_t *sbString, const sbstring_t *delimiter, int limit, sbarray_t *array);
sbstring_t *sbStringSubString(const sbstring_t *sbString, int start, int end, sbstring_t *newString);
char *sbStringToCString(const sbstring_t *sbString);
#endif /* SBSTRING_H */
