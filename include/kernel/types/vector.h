#ifndef KERNEL_TYPES_VECTOR_H
#define KERNEL_TYPES_VECTOR_H

#include <util.h>
#include <stddef.h>
#include <kernel/error.h>

/** A pointer to the ith item in the vector
  @param v The vector.
  @param i The index of the item.
*/

#define VECTOR_ITEM(v, i) ({ __typeof(v) _v=(v); (void *)((uintptr_t)_v->data + (i) * _v->item_size); })

/** A resizable array that dynamically expands as items are added
to it.
*/
typedef struct vec {
  /// The pointer to the data buffer.
  void *data;

  /// The size, in bytes, of each item
  size_t item_size;

  /// The maximum number of items the vector can hold without resizing.
  uint32_t capacity;

  /// The number of items contained in the vector.
  uint32_t count;
} vector_t;

/**
  Initialize a vector by allocating memory to hold items.

  @param vector The vector to be initialized.
  @param item_size The size, in bytes, of each item. Must be non-zero.
  @return E_OK, if successful. E_FAIL, on failure.
*/

NON_NULL_PARAMS int vector_init(vector_t *v, size_t item_size);

/**
  Initialize a vector with a specified capacity.

  @param vector The vector to be initialized.
  @param capacity The number of items for which the buffer should initially
  be capable of storing. If the capacity is zero, then no memory will be
  allocated. capacity must not be greater than than UINT32_MAX.
  @param item_size The size, in bytes, of each item. Must be non-zero.
  @return E_OK, if successful. E_FAIL, on failure.
*/

NON_NULL_PARAMS int vector_init_with_capacity(vector_t *v, size_t capacity, size_t item_size);

/**
  Destroy a vector by releasing all allocated memory.

  @param vector The vector to be destroyed.
  @return E_OK, on success. E_FAIL, on failure.
*/

NON_NULL_PARAMS int vector_release(vector_t *v);

/**
  Retrieves a copy of an item from a vector at the specified index.

  @param vector The vector.
  @param item A pointer to be used to store the retrieved item.
  If item is NULL, no data will be copied. Pointer must point to a
  valid memory object at least as large as the item's size.
  @param index The location of the item in the vector. Index must
  be less than the vector's item count.
  @return E_OK, on succes. E_NOT_FOUND if greater than or equal to
  the vector's item count. E_FAIL, if index is greater than the maximum
  vector capacity.
*/

NON_NULL_PARAM(1) int vector_get_copy(const vector_t * restrict vector, size_t index, void * restrict item);

/**
  Place an item into a vector at the specified index.

  @param vector The vector.
  @param index The item's location in the vector. If the index is equal to the
  vector's item count, then the vector will increase so that the new item will fit.
  @param item The item to be placed (must not be NULL).
  @return E_OK, on success. E_FAIL, on failure.
*/

NON_NULL_PARAMS int vector_set(vector_t * restrict vector, size_t index, const void * restrict item);

/**
  Provide a hint to resize a vector's buffer if it's too large.

  @param vector The vector to be resized.
  @return E_FAIL, if the buffer needed to be shrunk but the operation
  failed. E_OK, if the buffer successfully shrank or the buffer
  did not need to be resized.
*/

NON_NULL_PARAMS int vector_shrink_to_fit(vector_t *vector);

/**
  Insert a new item into the vector at a specified index. Any existing items
  at indicies greater than or equal to the specified index are shifted forward by one.

  @param vector The vector.
  @param index The index at which to insert the new item. Index must not be greater
  than the number of items in the vector.
  @param item The item to be inserted (must not be NULL).
  @return E_OK, if the item was inserted successfully. E_FAIL, if the index is invalid
  or if the vector was not able to increase in size.
*/

NON_NULL_PARAMS int vector_insert(vector_t * restrict vector, size_t index, const void * restrict item);

/**
  Remove an item from the vector at a specified index. Any existing items
  at indicies greater than or equal to the specified index are shifted backward by one.

  @param vector The vector.
  @param index The index at which to remove an new item.
  @param item The removed item. If NULL, removed item will be ignored.
  @return E_OK, on success. E_FAIL, if vector is empty.
*/

NON_NULL_PARAM(1) int vector_remove(vector_t * restrict vector, size_t index, void * restrict item);

/**
  Return the index of the first occurrence of an item in a vector, if it exists.

  @param vector The vector.
  @param item The item to search for.
  @return The index of the item, if found. E_NOT_FOUND if the item
  wasn't found in the vector.
*/

NON_NULL_PARAMS PURE int vector_index_of(const vector_t *restrict vector, const void * restrict item);

/**
  Return the index of the last occurrence of an item in a vector, if it exists.

  @param vector The vector.
  @param item The item to search for.
  @return The index of the item, if found. E_NOT_FOUND if the item
  wasn't found in the vector.
*/

NON_NULL_PARAMS PURE int vector_rindex_of(const vector_t *restrict vector, const void * restrict item);

/**
  Return the number of items in a vector.
  @param vector The vector.
  @return The number of items in the vector.
*/
NON_NULL_PARAMS PURE static inline size_t vector_get_count(const vector_t *vector) {
  return (size_t)vector->count;
}

/**
  Return the current capacity of a vector.
  @param vector The vector.
  @return The current item capacity of the vector.
*/

NON_NULL_PARAMS PURE static inline size_t vector_get_capacity(const vector_t *vector) {
  return (size_t)vector->capacity;
}

/**
  Return a vector's item size.
  @param vector The vector.
  @return The size of each item, in bytes.
*/

NON_NULL_PARAMS PURE static inline size_t vector_get_item_size(const vector_t *vector) {
  return (size_t)vector->item_size;
}

/**
  Append an item to the end of a vector.
  @param vector The vector.
  @param item The item to be added to the vector.
  @return E_OK, on success. E_FAIL, on failure
*/

NON_NULL_PARAMS static inline int vector_push_back(vector_t * restrict vector, const void * restrict item) {
  return vector_insert(vector, vector->count, item);
}

/**
  Insert an item to the front of a vector, shifting all other items forward by one.
  @param vector The vector.
  @param item The item to be added to the vector.
  @return E_OK, on success. E_FAIL, on failure
*/

NON_NULL_PARAMS static inline int vector_push_front(vector_t * restrict vector, const void * restrict item) {
  return vector_insert(vector, 0, item);
}

/**
  Removes an item from the end of a vector.
  @param vector The vector.
  @param item The removed item. If NULL, the removed item will be ignored.
  @return E_OK, on success. E_FAIL, if the vector is empty.
*/

NON_NULL_PARAM(1) static inline int vector_pop_back(vector_t * restrict vector, void * restrict item) {
  return vector->count ? vector_remove(vector, vector->count-1, item) : E_FAIL;
}

/**
  Removes an item from the front of a vector, shifting all other items backward by one.
  @param vector The vector.
  @param item The removed item. If NULL, the removed item will be ignored.
  @return E_OK, on success. E_FAIL, if the vector is empty.
*/

NON_NULL_PARAM(1) static inline int vector_pop_front(vector_t * restrict vector, void * restrict item) {
  return vector_remove(vector, 0, item);
}

/**
  Removes the first occurrence of an item from a vector.
  @param vector The vector.
  @param item The item to be removed.
  @return E_OK, on success. E_NOT_FOUND, if no occurrences are item
  exist in the vector. E_FAIL, on failure.
*/

NON_NULL_PARAMS static inline int vector_remove_item(vector_t * restrict vector, const void * restrict item) {
  int i = vector_index_of(vector, item);

  return IS_ERROR(i) ? i : vector_remove(vector, (size_t)i, NULL);
}

/**
  Removes the last occurrence of an item from a vector.
  @param vector The vector.
  @param item The item to be removed.
  @return E_OK, on success. E_NOT_FOUND, if no occurrences are item
  exist in the vector. E_FAIL, on failure.
*/

NON_NULL_PARAMS static inline int vector_rremove_item(vector_t * restrict vector, const void * restrict item) {
  int i = vector_rindex_of(vector, item);

  return IS_ERROR(i) ? i : vector_remove(vector, (size_t)i, NULL);
}

#endif /* KERNEL_TYPES_VECTOR_H */
