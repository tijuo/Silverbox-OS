#include <os/allocator.h>
#include <stdlib.h>
#include <string.h>

// TODO: Make the global allocator thread-safe
static ByteSlice global_alloc(void *data, AllocatorFunctionName alloc_func, size_t size, size_t alignment, void *old_ptr, size_t old_size);

static ByteSlice global_alloc(void *data, AllocatorFunctionName alloc_func, size_t size, size_t alignment, void *old_ptr, size_t old_size) {
    switch(alloc_func) {
        case ALLOC: {
            void *ptr = malloc(size);
            return (ByteSlice) { .ptr = ptr, .length = ptr ? size : 0 };
        }
        case ALLOC_ZERO: {
            void *ptr = calloc(size, 1);
            return (ByteSlice) { .ptr = ptr, .length = ptr ? size : 0 };
        }
        case RESIZE:
        case RESIZE_ZERO: {
            void *ptr = realloc(old_ptr, size);
            ByteSlice new_slice = (ByteSlice) { .ptr = ptr ? ptr : old_ptr, .length = ptr ? size : old_size };

            if(alloc_func == RESIZE_ZERO && ptr && size > old_size) {
                memset(&new_slice.ptr[old_size], 0, size - old_size);
            }

            return new_slice;
        }
        case FREE: {
            free(old_ptr);
            return NULL_SLICE(ByteSlice);
        }
        case FREE_ALL:
        default:
            break;
    }

    return NULL_SLICE(ByteSlice);
}

Allocator GLOBAL_ALLOCATOR = {
    .data = NULL,
    .func = global_alloc
};

Allocator NULL_ALLOCATOR = {
    .data = NULL,
    .func = NULL
};

ByteSlice allocator_alloc(Allocator *allocator, size_t size, size_t alignment) {
    if(!allocator->func) {
        return NULL_SLICE(ByteSlice);
    }

    return allocator->func(allocator->data, ALLOC, size, alignment, NULL, 0);
}

ByteSlice allocator_alloc_zeroed(Allocator *allocator, size_t size, size_t alignment) {
    if(!allocator->func) {
        return NULL_SLICE(ByteSlice);
    }

    return allocator->func(allocator->data, ALLOC_ZERO, size, alignment, NULL, 0);
}

ByteSlice allocator_resize(Allocator *allocator, void *old_ptr, size_t old_size, size_t new_size) {
    if(!allocator->func) {
        return NULL_SLICE(ByteSlice);
    }

    return allocator->func(allocator->data, RESIZE, new_size, 0, old_ptr, old_size);
}

ByteSlice allocator_resize_zeroed(Allocator *allocator, void *old_ptr, size_t old_size, size_t new_size) {
    if(!allocator->func) {
        return NULL_SLICE(ByteSlice);
    }

    return allocator->func(allocator->data, RESIZE_ZERO, new_size, 0, old_ptr, old_size);
}

void allocator_free(Allocator *allocator, void *ptr) {
    if(allocator->func) {
        allocator->func(allocator->data, FREE, 0, 0, ptr, 0);
    }
}

void allocator_free_all(Allocator *allocator) {
    if(allocator->func) {
        allocator->func(allocator->data, FREE_ALL, 0, 0, NULL, 0);
    }
}