#ifndef SBSTRING_H
#define SBSTRING_H

#include <stdlib.h>
#include <os/ostypes/sbarray.h>

struct _SBString
{
  unsigned char *data;
  int length;
  int charWidth;
};

typedef struct _SBString SBString;

enum SBStringResults { SBStringError=-1, SBStringFailed=-2, 
       SBStringBadCharWidth=-3, SBStringNotFound=-4 };

enum SBStringCompareResults { SBStringCompareError=-2, SBStringCompareLess=-1, 
                   SBStringCompareEqual=0, SBStringCompareGreater=1 };

int sbStringCharAt(const SBString *str, int index, void *c);
int sbStringCharWidth(const SBString *str);
int sbStringCompare(const SBString *str1, const SBString *str2);
int sbStringConcat(SBString *str, const SBString *addend);
int sbStringCopy(const SBString *str, SBString *newStr);
int sbStringCreate(SBString *sbString, const char *str, int width);
int sbStringCreateN(SBString *sbString, const char *str, size_t len, int width);
int sbStringDelete(SBString *sbString);
int sbStringFind(const SBString *haystack, const SBString *needle);
int sbStringFindChar(const SBString *sbString, int c);
int sbStringLength(const SBString *str);
int sbStringSplit(const SBString *sbString, const SBString *delimiter, 
                  int limit, SBArray *array);
int sbStringSubString(const SBString *sbString, int start, int end, 
                      SBString *newString);
int sbStringToCString(SBString *sbString, char **cString);
#endif /* SBSTRING_H */
