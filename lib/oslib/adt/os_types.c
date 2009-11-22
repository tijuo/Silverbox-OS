#include <os/os_types.h>

int createSBString(const char *str, SBString *sbString)
{
  if( sbString == NULL )
    return -1;
  
  sbString->charWidth = 1;

  if( str == NULL )
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

int concatSBString(SBString *dest, const SBString *src)
{
  void *buf;

  if( src == NULL || dest == NULL || src->data == NULL )
    return -1;
  else if( src->charWidth != dest->charWidth )
    return -2;
  else
  {
    if( dest->data == NULL )
      dest->length = 0;

    buf = malloc( src->charWidth * (dest->length + src->length) )
    strncpy( buf, dest->data, dest->length * dest->charWidth );
    strncpy( (unsigned)buf + dest->length * dest->charWidth, 
             src->data, src->length * src->charWidth );

    if( dest->data == NULL )
      free(dest->data);

    dest->data = buf;
    dest->length += src->length * src->charWidth;
  }

  return 0;
}
