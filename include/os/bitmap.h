#ifndef BITMAP_H
#define BITMAP_H

#include <stddef.h>

#define bitIsSet( bitmap, bit ) ( ( bitmap[bit >> 3] & (1 << (bit & 0x07)) ) > 0 ? true : false )
#define setBitmapBit( bitmap, bit ) ( bitmap[bit >> 3] |= (1 << (bit & 0x07)) )
#define clearBitmapBit( bitmap, bit ) ( bitmap[bit >> 3] &= (1 << (bit & 0x07)) )

unsigned char *createBitmap(size_t num_entries);
int initBitmap(unsigned char *bitmap, size_t numEntries);
void destroyBitmap(unsigned char *bitmap);

#endif /* BITMAP_H */
