#include <os/os_types.h>

int createSBString(const char *str, SBString *sbString)
{
  if( sbString == NULL )
    return -1;
  
  sbString->charWidth = 1;

  if( str == NULL || strlen(str) == 0 )
  {
    sbString->data = NULL;
    sbString->length = 0;
  }
  else
  {
    sbString->length = strlen(str);
    sbString->data = malloc(sbString->length);
    strncpy(sbString->data, str, sbString->length);
  }

  return 0;
}

void deleteSBString(SBString *sbString)
{
  if( sbString == NULL )
    return;
  else
  {
    if( sbString->data == NULL )
      free(sbString->data);

    sbString->length = 0;
  }
}

int concatSBString(SBString *str, const SBString *addend)
{
  void *buf;

  if( !addend || !str == NULL || (!str->data && str->len) || (!addend->data && addend->len) )
    return -1;
  else if( addend->charWidth != str->charWidth )
    return -2;

  str->data = realloc( str->data, (str->len + addend->len) * addend->charWidth );

  if( str->data == NULL )
  {
    str->len = 0;
    return (str->len + addend->len == 0) ? 0 : -1;
  }

  strncpy( str->data + str->length * str->charWidth,
           addend->data, addend->length * addend->charWidth );

  str->length += addend->length * addend->charWidth;

  return 0;
}
