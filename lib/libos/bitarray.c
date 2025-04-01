#include <os/bitarray.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <limits.h>

unsigned char* bitarray_new(size_t num_bits)
{
    return calloc(num_bits / CHAR_BIT + !!(num_bits % CHAR_BIT), 1);
}

int bitarray_init(unsigned char* bitarray, size_t num_entries)
{
    size_t length = num_entries / CHAR_BIT + ((num_entries % CHAR_BIT) ? 1 : 0);

    if(!length) {
        return -1;
    }

    memset(bitarray, 0, length);
    return 0;
}

void bitarray_destroy(unsigned char* bitarray)
{
    free(bitarray);
}
