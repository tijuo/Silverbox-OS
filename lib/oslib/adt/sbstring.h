#ifndef SBSTRING_H
#define SBSTRING_H

struct _SBString
{
  unsigned char *data;
  int length;
  int charWidth;
};

typedef struct _SBString SBString;

enum SBStringResults { SBStringError=-1, SBStringFailed=-2, 
       SBStringBadCharWidth=-3, SBStringNotFound=-4 };

int sbStringCharAt(const SBString *str, int index, void *c);
int sbStringCharWidth(const SBString *str);
int sbStringConcat(SBString *str, const SBString *addend);
int sbStringCopy(const SBString *str, SBString *newStr);
int sbStringCreate(SBString *sbString, const char *str, int width);
int SBStringDelete(SBString *sbString);
int SBStringLength(const SBString *str);

#endif /* SBSTRING_H */
