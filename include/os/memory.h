#ifndef OS_MEMORY_H
#define OS_MEMORY_H

#include <types.h>

#define HEAP_START 0x10000000
#define HEAP_LIMIT 0xB0000000
#define PAGE_SIZE  4096

void *heapStart, *heapEnd;
size_t heapSize;

void *sbrk( int increment );

#endif /* OS_MEMORY_H */
