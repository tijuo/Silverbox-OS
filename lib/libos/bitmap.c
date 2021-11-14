#include <os/bitmap.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <limits.h>

unsigned char* createBitmap(size_t numEntries) {
  return calloc(numEntries / CHAR_BIT + ((numEntries % CHAR_BIT) ? 1 : 0),
                (size_t)(CHAR_BIT / 8));
}

int initBitmap(unsigned char *bitmap, size_t numEntries) {
  size_t bitmapLen = numEntries / CHAR_BIT + ((numEntries % CHAR_BIT) ? 1 : 0);

  if(!bitmap || !bitmapLen)
    return -1;

  memset(bitmap, 0, bitmapLen);
  return 0;
}

void destroyBitmap(unsigned char *bitmap) {
  free(bitmap);
}
