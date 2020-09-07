#ifndef MEMORY_H
#define MEMORY_H

#include <types.h>

void *memcpy( void *dest, const void *src, size_t count);
void *memset( void *buffer, int c, size_t num);

/* This won't work with arrays that have been passed into functions or coverted into pointers. */

#define ARRAY_SIZE(arr)     (sizeof(arr) / sizeof(arr[0]))

#endif /* MEMORY_H */
