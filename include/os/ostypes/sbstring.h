#ifndef SBSTRING_H
#define SBSTRING_H

#include <stdlib.h>
#include <os/ostypes/dynarray.h>
#include <os/ostypes/ostypes.h>

typedef struct sbstring_t
{
  char *data;
  size_t length;
  size_t charWidth;
} sbstring_t;

char sbStringCharAt(sbstring_t *str, size_t index);
size_t sbStringCharWidth(sbstring_t *str);
int sbStringCompare(sbstring_t *str1, sbstring_t *str2);
sbstring_t *sbStringConcat(sbstring_t *str, sbstring_t *addend);
sbstring_t *sbStringCopy(sbstring_t *str, sbstring_t *newStr);
sbstring_t *sbStringCreate(sbstring_t *sbString, char *str);
sbstring_t *sbStringCreateN(sbstring_t *sbString, char *str, size_t len);
int sbStringDelete(sbstring_t *sbString);
int sbStringFind(sbstring_t *haystack, sbstring_t *needle);
int sbStringFindChar(sbstring_t *sbString, int c);
size_t sbStringLength(sbstring_t *str);
DynArray *sbStringSplit(sbstring_t *sbString, sbstring_t *delimiter, int limit, DynArray *array);
sbstring_t *sbStringSubString(sbstring_t *sbString, int start, int end, sbstring_t *newString);
char *sbStringToCString(sbstring_t *sbString);
#endif /* SBSTRING_H */
