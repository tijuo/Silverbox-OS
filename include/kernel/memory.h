#ifndef KERNEL_MEMORY_H
#define KERNEL_MEMORY_H

#include <types.h>

void clearMemory(void *ptr, size_t len);
void *memcpy( void *dest, const void *src, size_t count);
void *memset( void *buffer, int c, size_t num);

/* This won't work with arrays that have been passed into functions or converted into pointers. */

#define ARRAY_SIZE(arr)     (sizeof(arr) / sizeof(arr[0]))

#endif /* KERNEL_MEMORY_H */
