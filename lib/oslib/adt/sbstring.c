#include "sbstring.h"
#include <string.h>
#include <stdlib.h>

int sbStringCharAt(const SBString *str, int index, void *c)
{
  if( !str || index < 0 || index >= str->length || !c )
    return SBStringError;

  memcpy(c, (void *)(str->data + index * str->charWidth), str->charWidth );
  return 0;
}

int sbStringCharWidth(const SBString *str)
{
  if( !str )
    return SBStringError;

  return str->charWidth;
}

int sbStringConcat(SBString *str, const SBString *addend)
{
  void *buf;

  if( !addend || !str )
    return SBStringError;
  else if( addend->charWidth != str->charWidth )
    return SBStringBadCharWidth;

  buf = realloc( str->data, (str->length + addend->length) * 
                 addend->charWidth );

  if( buf == NULL )
  {
    if( str->length + addend->length == 0 )
      return 0;
    else
      return SBStringFailed;
  }
  else
    str->data = buf;

  memcpy( str->data + str->length * str->charWidth,
           addend->data, addend->length * addend->charWidth );

  str->length += addend->length;

  return 0;
}

int sbStringCopy(const SBString *str, SBString *newStr)
{
  if( !str || !newStr )
    return SBStringError;

  newStr->charWidth = str->charWidth;

  newStr->data = malloc(str->length * str->charWidth);

  if( newStr->data == NULL )
  {
    newStr->length = 0;
    return SBStringFailed;
  }

  newStr->length = str->length;
  return 0;
}

int sbStringCreate(SBString *sbString, const char *str, int width)
{
  size_t len;

  if( sbString == NULL || width < 1 )
    return SBStringError;
  
  sbString->charWidth = width;

  len = strlen(str);

  if( str == NULL || len == 0 )
  {
    sbString->data = NULL;
    sbString->length = 0;
  }
  else
  {
    sbString->length = len;
    sbString->data = malloc(sbString->length*width);
    memcpy(sbString->data, str, sbString->length*width);
  }

  return 0;
}

int sbStringDelete(SBString *sbString)
{
  if( sbString == NULL )
    return SBStringError;

  if( sbString->data != NULL )
    free(sbString->data);
  sbString->length = 0;

  return 0;
}

int sbStringLength(const SBString *str)
{
  if(!str)
    return SBStringError;
  
  return str->length;
}
