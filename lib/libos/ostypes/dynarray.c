#include <os/ostypes/dynarray.h>
#include <os/ostypes/ostypes.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <limits.h>
#include <stdint.h>

#define DEFAULT_CAPACITY        8

static void shift_array(DynArray *array, size_t starting_index, int shift);
static int expand_array(DynArray* array);
static int get_position(DynArray* array, int pos);

#define ARRAY_PTR(arr, i)   (void *)((uintptr_t)(arr)->elems + (i) * (arr)->elem_size)

static int get_position(DynArray* array, int pos)
{
    if(pos > (int)array->array_size) {
        pos = (int)array->array_size;
    } else if(pos < -(int)array->array_size) {
        pos = 0;
    }

    return pos < 0 ? ((int)array->array_size + pos) : pos;
}

static void shift_array(DynArray *array, size_t starting_index, int shift)
{
    memmove(ARRAY_PTR(array, (size_t)((int)starting_index + shift)), ARRAY_PTR(array, starting_index), (int)array->array_size-(int)starting_index);
}

static int expand_array(DynArray* array)
{
    unsigned long new_capacity;

    if(array->array_size == array->capacity && array->capacity > 8) {
        new_capacity = (3 * (unsigned long)array->array_size) / 2;
    } else {
        new_capacity = (unsigned long)array->array_size + 3;
    }

    if((int)new_capacity < 0) {
        new_capacity = (unsigned long)INT_MAX;
    }

    ByteSlice new_data = allocator_resize(&array->allocator, array->elems, array->capacity * array->elem_size, (size_t)new_capacity * array->elem_size);
    
    if(!new_data.ptr) {
        return SB_FAIL;
    }

    array->capacity = (size_t)new_capacity;
    array->elems = (void *)new_data.ptr;

    return 0;
}

/**
 * Remove all elements in a dynamic array.
 * 
 * The underlying memory isn't necessarily altered. An explicit call to `dyn_array_shrink()`,
 * would need to be called to resize the storage.
 * 
 * @param array The dynamic array.
 */
void dyn_array_clear(DynArray* array)
{
    array->array_size = 0;
}

/**
 * Reduce the memory capacity of a dynamic array to fit the number of elements in the array.
 * 
 * @param array The dynamic array.
 * @return `0`, on success. `SB_FAIL`, on failure. `SB_NO_OP`, if no operation was performed
 * (because the dynamic array doesn't need to be shrunk).
 */
int dyn_array_shrink(DynArray* array) {
    if(array->capacity > array->array_size) {
        ByteSlice slice = allocator_resize(&array->allocator, array->elems, array->capacity * array->elem_size, array->array_size * array->elem_size);

        if(!slice.ptr) {
            return SB_FAIL;
        }

        array->elems = (void *)slice.ptr;
        array->capacity = array->array_size;

        return 0;
    } else {
        return SB_NO_OP;
    }
}

/**
 * Create a copy of the elements of a dynamic array.
 * 
 * @param array The dynamic array to be cloned.
 * @param new_array The new dynamic array.
 * @return The cloned dynamic array, on success. `NULL`, on failure.
 */
DynArray* dyn_array_clone(const DynArray* array, DynArray* new_array)
{
    if(dyn_array_init(new_array, array->elem_size, array->capacity, &array->allocator) != 0) {
        return NULL;
    }

    memcpy(new_array->elems, ARRAY_PTR(array, array->array_size), array->elem_size);
    return new_array;
}

/**
 * Return the number of elements in a dynamic array.
 * 
 * @param array The dynamic array.
 * @return The length of the dynamic array.
 */
size_t dyn_array_length(const DynArray* array)
{
    return array->array_size;
}

/**
 * Initialize a dynamic array.
 * 
 * @param array The dynamic array to be initialized.
 * @param elem_size The size of each element, in bytes. Must be non-zero.
 * @param capacity The initial capacity of the dynamic array, in number of elements. If zero, then no memory will be allocated.
 * @param allocator The allocator to use for the backing buffer. If `NULL`, then the null allocator will be used.
 */
int dyn_array_init(DynArray* array, size_t elem_size, size_t capacity, Allocator *allocator)
{
    if(elem_size == 0) {
        return SB_FAIL;
    }

    array->array_size = 0;
    array->elem_size = elem_size;

    if(!allocator) {
        array->allocator = NULL_ALLOCATOR;
        array->capacity = 0;
        array->elems = NULL;
    } else {
        ByteSlice slice = allocator_alloc(allocator, capacity * elem_size, sizeof(int));

        if(!slice.ptr) {
            return SB_FAIL;
        }

        array->capacity = capacity;
        array->allocator = *allocator;
        array->elems = (void *)slice.ptr;
    }

    return 0;
}

/**
 * Deinitialize the dynamic array, freeing any allocated resources.
 * 
 * @param array The dynamic array.
 */
void dyn_array_destroy(DynArray* array)
{
    if(array->elems) {
        allocator_free(&array->allocator, array->elems);
    }
    array->elems = NULL;
    array->capacity = 0;
    array->array_size = 0;
}

/**
 * Retrieve an element in the array.
 * 
 * @param array The dynamic array.
 * @param pos The element position. If pos is negative, then the item is relative to the array length
 * (i.e. -1 is the last element, -2 is the second to last, etc.).
 * @param elem A out pointer to an element pointer. This is where the pointer to the located element
 * will be stored.
 * @return `0`, if found. `SB_NOT_FOUND`, if `pos` is out of range.
 */
int dyn_array_get(const DynArray* array, int pos, void** elem)
{
    pos = get_position(array, pos);

    if(pos >= (int)array->array_size || pos < 0) {
        return SB_NOT_FOUND;
    }

    if(elem) {
        *elem = ARRAY_PTR(array, pos);
    }

    return 0;
}

/** Returns the index of the first occurence of an element in a dynamic array (if found).
 * 
 * @param array The dynamic array.
 * @param elem The element to locate.
 * @param cmp A comparator function that determines whether two elements are equal or not. If `cmp`
 * returns `0` when an element is compared to `elem`, then it will be assumed that `elem` was
 * found. `cmp` returning any other value indicates a non-match.
 * @return A non-negative index indicating the position of the located element. `SB_NOT_FOUND`, if
 * the element was not found.
 */
int dyn_array_find(const DynArray* array, void* elem, int (*cmp)(void* x, void* y))
{
    for(size_t i = 0; i < array->array_size; i++) {
        if(cmp(elem, (void *)((uintptr_t)array->elems + i * array->elem_size)) == 0) {
            return (int)i;
        }
    }

    return SB_NOT_FOUND;
}

/**
 * Insert an element into a location in the array.
 * 
 * @param array The dynamic array.
 * @param pos The element position. If pos is negative, then the item is relative to the array length
 * (i.e. -1 is the last element, -2 is the second to last, etc.).
 * @param ptr A pointer to the element to be inserted. Must not be `NULL`.
 * @return `0`, on success. `SB_FAIL`, on failure.
 */
int dyn_array_insert(DynArray* array, int pos, void* ptr)
{
    pos = get_position(array, pos);

    if(pos < 0 || !ptr || pos > (int)array->array_size || (int)array->array_size == INT_MAX) {
        return SB_FAIL;
    }

    if(array->array_size == array->capacity) {
        if(expand_array(array) != 0) {
            return SB_FAIL;
        }
    }

    shift_array(array, pos, 1);

    memcpy(ARRAY_PTR(array, (size_t)pos), ptr, array->elem_size);
    array->array_size++;

    return 0;
}

/**
 * Removes an element from the end of a dynamic array.
 *
 * @param array The dynamic array.
 * @param ptr An out pointer for the popped-element. If `NULL`, then the element will simply be discarded.
 * @return `0`, if the pop was successful. `SB_EMPTY`, if the pop failed because the
 * dynamic array was empty.
 */
int dyn_array_pop(DynArray* array, void* ptr)
{
    if(array->array_size == 0) {
        return SB_EMPTY;
    }

    array->array_size -= 1;

    if(ptr) {
        memcpy(ptr, ARRAY_PTR(array, array->array_size), array->array_size);
    }

    return 0;
}

/**
 * Adds an element onto the end of a dynamic array.
 *
 * @param array The dynamic array.
 * @param ptr A pointer to the element to be pushed. Must not be `NULL`.
 * @return `0`, if the pop was successful. `SB_EMPTY`, if the pop failed because the
 * dynamic array was empty.
 */
int dyn_array_push(DynArray* array, void* ptr)
{
    if(!ptr || array->array_size == INT_MAX || (array->array_size == array->capacity && expand_array(array) != 0)) {
        return SB_FAIL;
    }

    memcpy(ARRAY_PTR(array, array->array_size), ptr, array->elem_size);
    array->array_size += 1;

    return 0;
}

/**
 * Remove an element at a specified position in a dynamic array.
 * 
 * @param array The dynamic array.
 * @param pos The element position.
 * @return `0`, on success. `SB_NOT_FOUND` if no element exists at the specified position.
 */
int dyn_array_remove(DynArray* array, int pos)
{
    pos = get_position(array, pos);

    if(pos >= (int)array->array_size || pos < 0)
        return SB_NOT_FOUND;

    shift_array(array, pos, -1);
    array->array_size -= 1;

    return 0;
}

/**
 * Remove an element at a specified position in a dynamic array without preserving the order of elements.
 * 
 * @param array The dynamic array.
 * @param pos The element position.
 * @return `0`, on success. `SB_NOT_FOUND` if no element exists at the specified position.
 */
int dyn_array_remove_unstable(DynArray* array, int pos)
{
    pos = get_position(array, pos);

    if(pos >= (int)array->array_size || pos < 0) {
        return SB_NOT_FOUND;
    }

    if(pos + 1 < (int)array->array_size) {
        memcpy(ARRAY_PTR(array, (size_t)pos), ARRAY_PTR(array, array->array_size - 1), array->elem_size);
    }

    array->array_size -= 1;

    return 0;
}

/** Returns the subarray within a dynamic array between positions 'start' (inclusive) and 'end' (non-inclusive).
 * 
 * @param array The dynamic array.
 * @param start The starting index.
 * @param end The ending index.
 * @param new_array The sub array.
 * @return `0`, on success. `SB_FAIL`, on failure.
 */
int dyn_array_subarray(DynArray* array, int start, int end, DynArray* new_array)
{
    start = get_position(array, start);
    end = get_position(array, end);
    size_t new_size = (end < start ? 0 : end - start);

    if(dyn_array_init(new_array, array->elem_size, new_size, &array->allocator) != 0) {
        return SB_FAIL;
    }

    memcpy(new_array->elems, ARRAY_PTR(array, start), new_size * array->elem_size);
    new_array->array_size = new_size;

    return 0;
}
