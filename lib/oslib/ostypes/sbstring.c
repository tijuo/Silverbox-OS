#include <os/ostypes/sbstring.h>
#include <string.h>
#include <stdlib.h>

int sbStringCharAt(const SBString *str, int index, void *c)
{
  if( !str || index < 0 || index >= str->length || !c )
    return SBStringError;

  memcpy(c, &str->data[index * str->charWidth], str->charWidth );
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

  memcpy( &str->data[str->length * str->charWidth],
           addend->data, addend->length * addend->charWidth );

  str->length += addend->length;

  return str->length;
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

  memcpy(newStr->data, str->data, str->length * str->charWidth);
  newStr->length = str->length;
  return str->length;
}

int sbStringCreate(SBString *sbString, const char *str, int width)
{
  return sbStringCreateN(sbString, str, strlen(str), width);
}

int sbStringCreateN(SBString *sbString, const char *str, size_t len, int width)
{
  if( sbString == NULL || width < 1 )
    return SBStringError;
  
  sbString->charWidth = width;

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

  return sbString->length;
}

int sbStringCompare(const SBString *str1, const SBString *str2)
{
  int i=0;

  if( str1 == NULL || str2 == NULL || str1->charWidth != str2->charWidth )
    return SBStringCompareError;

  while( str1->data && str2->data && i < i < str1->length &&
         i < str2->length && memcmp(str1->data + i * str1->charWidth,
         str2->data + i * str2->charWidth, str2->charWidth ) == 0 )
  {
    i++;
  }


  if( str1->length == str2->length )
    return SBStringCompareEqual;
  else if( i < str1->length && i < str2->length )
  {
    return memcmp(str1->data + i * str1->charWidth,
         str2->data + i * str2->charWidth, str2->charWidth );
  }
  else if( i >= str1->length )
    return SBStringCompareLess;
  else
    return SBStringCompareGreater;
}

int sbStringDelete(SBString *sbString)
{
  if( sbString == NULL )
    return SBStringError;

  if( sbString->data != NULL )
  {
    free(sbString->data);
    sbString->data = NULL;
  }

  sbString->length = 0;

  return 0;
}

int sbStringFind(const SBString *haystack, const SBString *needle)
{
  if( !haystack || !needle )
    return SBStringError;
  else if( haystack->charWidth != needle->charWidth )
    return SBStringBadCharWidth;

  if( needle->length == 0 )
    return 0;

  for(int i=0; i < haystack->length - needle->length - 1; i++)
  {
    if( memcmp(haystack->data + i * haystack->charWidth, 
               needle->data, needle->length * needle->charWidth) == 0 )
    {
      return i;
    }
  }

  return SBStringNotFound;
}

int sbStringFindChar(const SBString *sbString, int c)
{
  if( !sbString )
    return SBStringError;

  for(int i=0; i < sbString->length; i++)
  {
    if( memcmp(&sbString->data[i * sbString->charWidth], &c, 
           sbString->charWidth) == 0 )
    {
      return i;
    }
  }

  return SBStringNotFound;
}

int sbStringLength(const SBString *str)
{
  if(!str)
    return SBStringError;

  return str->length;
}

int sbStringSplit(const SBString *sbString, const SBString *delimiter, 
                  int limit, SBArray *array)
{
  int start=0, end=0;
  SBString *tempStr, endStr;

  if( !sbString || !array )
    return SBStringError;

  if( sbArrayCreate(array) != 0 )
    return SBStringFailed;

  while(1)
  {
    sbStringSubString(sbString, start, -1, &endStr);
    tempStr = malloc(sizeof(SBString));
    end = sbStringFind(&endStr, delimiter);

    if( (end == SBStringNotFound || limit == 0) && tempStr )
    {
      sbStringCopy(&endStr, tempStr);
      sbArrayPush(array,tempStr, sizeof *tempStr);
      sbStringDelete(&endStr);
      return 0;
    }
    else if( !tempStr || (end < 0 && end != SBStringNotFound) )
    {
      SBString *str;

      while( sbArrayCount(array) )
      {
        sbArrayPop(array, (void **)&str, NULL);
        sbStringDelete(str);
      }

      if( tempStr )
        free(tempStr);

      sbStringDelete(&endStr);
      return SBStringFailed;
    }
    else
    {
      sbStringSubString(&endStr, 0, end, tempStr);
      sbArrayPush(array, tempStr, sizeof *tempStr);
      start += end + delimiter->length;

      if( limit > 0 )
        limit--;

      sbStringDelete(&endStr);
    }
  }

  return SBStringFailed;
}

// start (inclusive), end (non-inclusive)

int sbStringSubString(const SBString *sbString, int start, int end, 
                      SBString *newString)
{
  if( !sbString || !newString )
    return SBStringError;

  if( start < 0 )
    start = 0;

  if( end < 0 )
    end = sbString->length;

  newString->data = NULL;
  newString->charWidth = sbString->charWidth;
  newString->length = 0;

  if( end - start > 0 )
  {
    newString->data = malloc((end-start)*newString->charWidth);

    if( !newString->data )
      return SBStringFailed;

    newString->length = end - start;
  }
  else
    return 0;

  memcpy(newString->data, &sbString->data[start*sbString->charWidth], 
         newString->length * sbString->charWidth);
  return newString->length;
}

int sbStringToCString(SBString *sbString, char **cString)
{
  if( !sbString || !cString )
    return SBStringError;

  *cString = malloc(sbString->length * sbString->charWidth);

  if( !*cString )
    return SBStringFailed;

  memcpy( *cString, sbString->data, sbString->length * sbString->charWidth );
  return sbString->length * sbString->charWidth;
}
