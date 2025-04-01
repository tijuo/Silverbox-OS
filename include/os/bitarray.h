#ifndef BITARRAY_H
#define BITARRAY_H

#include <stddef.h>

#define bitarray_is_set( bitarray, bit ) ( ( bitarray[bit >> 3] & (1 << (bit & 0x07)) ) > 0 ? true : false )
#define bitarray_set( bitarray, bit ) ( bitarray[bit >> 3] |= (1 << (bit & 0x07)) )
#define bitarray_clear( bitarray, bit ) ( bitarray[bit >> 3] &= (1 << (bit & 0x07)) )

unsigned char* bitarray_new(size_t num_bits);
int bitarray_init(unsigned char* bitarray, size_t num_entries);
void bitarray_destroy(unsigned char* bitarray);

#endif /* BITARRAY_H */
