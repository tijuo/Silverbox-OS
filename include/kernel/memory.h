#ifndef MEMORY_H
#define MEMORY_H

#include <types.h>

void *memcpy( void *dest, const void *src, size_t count);
void *memset( void *buffer, int c, size_t num);

#endif /* MEMORY_H */
