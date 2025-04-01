#ifndef OS_DYN_ARRAY_H
#define OS_DYN_ARRAY_H

#include <stddef.h>
#include <os/allocator.h>

typedef struct {
    void* elems;
    size_t array_size;
    size_t capacity;
    size_t elem_size;
    Allocator allocator;
} DynArray;

int dyn_array_init(DynArray* array, size_t elem_size, size_t capacity, Allocator *allocator);
void dyn_array_clear(DynArray* array);
DynArray* dyn_array_clone(const DynArray* array, DynArray* newArray);
size_t dyn_array_length(const DynArray* array);
void dyn_array_destroy(DynArray* array);
int dyn_array_get(const DynArray* array, int pos, void** elem);
int dyn_array_find(const DynArray* array, void* elem, int (*cmp)(void* x, void* y));
int dyn_array_insert(DynArray* array, int pos, void* ptr);
int dyn_array_pop(DynArray* array, void* elem);
int dyn_array_push(DynArray* array, void* ptr);
int dyn_array_remove(DynArray* array, int pos);
int dyn_array_remove_unstable(DynArray* array, int pos);
int dyn_array_subarray(DynArray* array, int start, int end, DynArray* newArray);
int dyn_array_shrink(DynArray *array);

#endif /* OS_DYN_ARRAY_H */