#include <os/bitmap.h>
#include <string.h>
#include <stddef.h>

int createBitmap( bitmap_t *bitmap, size_t entries )
{
  size_t bitmap_len;

  if( bitmap == NULL )
    return -1;

  bitmap_len = entries / 8;

  if( entries % 8 )
    bitmap_len++;

  memset( bitmap, 0, bitmap_len );
  return 0;
}
