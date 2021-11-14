#include <os/ostypes/sbstring.h>
#include <string.h>
#include <stdlib.h>

char sbStringCharAt(sbstring_t *str, size_t index) {
  if(!str || index >= str->length)
    return '\0';

  return str->data[index];
}

sbstring_t* sbStringConcat(sbstring_t *str, sbstring_t *addend) {
  void *buf;

  if(!addend || !str)
    return NULL;

  buf = realloc(str->data, str->length + addend->length);

  if(!buf) {
    if(str->length + addend->length == 0)
      return str;
    else
      return NULL;
  }
  else
    str->data = buf;

  memcpy(&str->data[str->length], addend->data, addend->length);

  str->length += addend->length;

  return str;
}

sbstring_t* sbStringCopy(sbstring_t *str, sbstring_t *newStr) {
  if(!str || !newStr)
    return NULL;

  newStr->data = malloc(str->length);

  if(!newStr->data)
    return NULL;

  memcpy(newStr->data, str->data, str->length);
  newStr->length = str->length;
  return newStr;
}

sbstring_t* sbStringCreate(sbstring_t *sbString, char *str) {
  return sbStringCreateN(sbString, str, strlen(str));
}

sbstring_t* sbStringCreateN(sbstring_t *sbString, char *str, size_t len) {
  if(!sbString)
    return NULL;

  if(!str || !len) {
    sbString->data = NULL;
    sbString->length = 0;
  }
  else {
    sbString->length = len;
    sbString->data = malloc(sbString->length);
    memcpy(sbString->data, str, sbString->length);
  }

  return sbString;
}

int sbStringCompare(sbstring_t *str1, sbstring_t *str2) {
  size_t i = 0;

  if(str1 == NULL || str2 == NULL)
    return SB_FAIL;

  if(str1->data && str2->data) {
    while(i < str1->length && i < str2->length && str1->data[i] == str2->data[i])
      i++;
  }

  if(str1->length == str2->length)
    return SB_EQUAL;
  else if(i < str1->length && i < str2->length)
    return memcmp(str1->data + i, str2->data + i, 1);
  else if(i >= str1->length)
    return SB_LESS;
  else
    return SB_GREATER;
}

int sbStringDelete(sbstring_t *sbString) {
  if(sbString == NULL)
    return SB_FAIL;

  if(sbString->data != NULL) {
    free(sbString->data);
    sbString->data = NULL;
  }

  sbString->length = 0;

  return 0;
}

int sbStringFind(sbstring_t *haystack, sbstring_t *needle) {
  if(!haystack || !needle || needle->length > haystack->length)
    return SB_FAIL;

  if(needle->length == 0)
    return 0;

  for(size_t i = 0; i <= haystack->length - needle->length; i++) {
    if(memcmp(haystack->data + i, needle->data, needle->length) == 0)
      return i;
  }

  return SB_NOT_FOUND;
}

int sbStringFindChar(sbstring_t *sbString, int c) {
  if(!sbString)
    return SB_FAIL;

  for(size_t i = 0; i < sbString->length; i++) {
    if(sbString->data[i] == c)
      return i;
  }

  return SB_NOT_FOUND;
}

size_t sbStringLength(sbstring_t *str) {
  if(!str)
    return 0;

  return str->length;
}

sbarray_t* sbStringSplit(sbstring_t *sbString, sbstring_t *delimiter, int limit,
                         sbarray_t *array)
{
  int start = 0, end = 0;
  sbstring_t tempStr, endStr;

  if(!sbString || !array)
    return NULL;

  if(sbArrayCreate(array) != 0)
    return NULL;

  while(1) {
    sbStringSubString(sbString, start, -1, &endStr);
    sbStringCreate(&tempStr, NULL);
    end = sbStringFind(&endStr, delimiter);

    if((end == SB_NOT_FOUND || limit == 0)) {
      if(endStr.length > 0) {
        sbStringCopy(&endStr, &tempStr);
        sbArrayPush(array, &tempStr);
      }
      else
        sbStringDelete(&tempStr);

      sbStringDelete(&endStr);
      return array;
    }
    else if((end < 0 && end != SB_NOT_FOUND)) {
      sbstring_t *str;

      while(sbArrayCount(array)) {
        sbArrayPop(array, (void**)&str);
        sbStringDelete(str);
      }

      sbStringDelete(&endStr);
      return NULL;
    }
    else {
      sbStringSubString(&endStr, 0, end, &tempStr);
      sbArrayPush(array, &tempStr);
      start += end + delimiter->length;

      if(limit > 0)
        limit--;

      sbStringDelete(&endStr);
    }
  }

  return NULL;
}

// start (inclusive), end (non-inclusive)

sbstring_t* sbStringSubString(sbstring_t *sbString, int start, int end,
                              sbstring_t *newString)
{
  if(!sbString || !newString)
    return NULL;

  if(start < 0)
    start = 0;

  if(end < 0)
    end = sbString->length;

  sbStringCreate(newString, NULL);

  if(end - start > 0) {
    newString->data = malloc(end - start);

    if(!newString->data)
      return NULL;

    newString->length = end - start;
  }
  else
    return newString;

  memcpy(newString->data, &sbString->data[start], newString->length);
  return newString;
}

char* sbStringToCString(sbstring_t *sbString) {
  char *str;

  if(!sbString)
    return NULL;

  str = malloc(sbString->length + 1);

  if(!str)
    return NULL;

  memcpy(str, sbString->data, sbString->length);
  str[sbString->length] = '\0';

  return str;
}
