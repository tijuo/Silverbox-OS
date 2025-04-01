#ifndef OS_ALLOCATOR_H
#define OS_ALLOCATOR_H

#include <stddef.h>
#include <os/ostypes/slice.h>

typedef enum {
    ALLOC,
    ALLOC_ZERO,
    RESIZE,
    RESIZE_ZERO,
    FREE,
    FREE_ALL
} AllocatorFunctionName;

typedef ByteSlice (*AllocatorFunction)(void *allocator_data, AllocatorFunctionName alloc_func, size_t size, size_t alignment, void *old_ptr, size_t old_size);

typedef struct {
    void *data;
    AllocatorFunction func;
} Allocator;

extern Allocator GLOBAL_ALLOCATOR;
extern Allocator NULL_ALLOCATOR;

ByteSlice allocator_alloc(Allocator *allocator, size_t size, size_t alignment);
ByteSlice allocator_alloc_zeroed(Allocator *allocator, size_t size, size_t alignment);
ByteSlice allocator_resize(Allocator *allocator, void *old_ptr, size_t old_size, size_t new_size);
ByteSlice allocator_resize_zeroed(Allocator *allocator, void *old_ptr, size_t old_size, size_t new_size);
void allocator_free(Allocator *allocator, void *ptr);
void allocator_free_all(Allocator *allocator);

#endif /* OS_ALLOCATOR_H */