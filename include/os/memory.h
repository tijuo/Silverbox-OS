#ifndef OS_MEMORY_H
#define OS_MEMORY_H

#include <types.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
extern const void * const __end;
extern size_t heapSize;
extern void *heapStart, *heapEnd;
void *sbrk( int increment );

#ifdef __cplusplus
}
#endif /* __cplusplus */

#define HEAP_START (&__end)
#define HEAP_LIMIT 0xB0000000
#define PAGE_SIZE  4096


#endif /* OS_MEMORY_H */
