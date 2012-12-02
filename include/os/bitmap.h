#ifndef BITMAP_H
#define BITMAP_H

#include <stddef.h>

typedef unsigned char bitmap_t;

#define bitIsSet( bitmap, bit ) ( ( bitmap[bit >> 3] & (1 << (bit & 0x07)) ) > 0 ? true : false )
#define setBitmapBit( bitmap, bit ) ( bitmap[bit >> 3] |= (1 << (bit & 0x07)) )
#define clearBitmapBit( bitmap, bit ) ( bitmap[bit >> 3] &= (1 << (bit & 0x07)) )

int createBitmap( bitmap_t *bitmap, size_t entries );

#endif /* BITMAP_H */
