#include <kernel/mm.h>
#include <kernel/types/vector.h>
#include <kernel/error.h>
#include <stdint.h>
#include <string.h>
#include <kernel/debug.h>
#include <kernel/memory.h>

#define DEFAULT_CAPACITY      4

_Static_assert(DEFAULT_CAPACITY <= LONG_MAX, "DEFAULT_CAPACITY cannot be larger than LONG_MAX");

/**
  Resize a vector's buffer.

  @param vector The vector.
  @param new_capacity The buffer's new capacity (in elements).
  @return E_OK, on success. EFAIL, on failure.
*/

static NON_NULL_PARAMS int vector_resize(vector_t *vector, size_t new_capacity);

int vector_init(vector_t *vector, size_t item_size) {
  return vector_init_with_capacity(vector, DEFAULT_CAPACITY, item_size);
}

int vector_init_with_capacity(vector_t *vector, size_t capacity, size_t item_size) {
  if(item_size == 0)
    RET_MSG(E_FAIL, "Item size must be non-zero");
  else if(capacity > MAX_CAPACITY)
    RET_MSG(E_FAIL, "Capacity must not be greater than LONG_MAX");

  vector->capacity = capacity;
  vector->item_size = item_size;
  vector->count = 0;

  if(capacity) {
    vector->data = kmalloc(capacity * item_size, 0);
    return vector->data ? E_OK : E_FAIL;
  }
  else
    return E_OK;
}

int vector_release(vector_t *vector) {
  if(vector->data)
    kfree(vector->data);

  vector->capacity = 0;
  vector->count = 0;
  vector->data = NULL;

  return E_OK;
}

int vector_get_copy(const vector_t * restrict vector, size_t index, void * restrict item) {
  if(index > MAX_CAPACITY)
    RET_MSG(E_FAIL, "Index must not be greater than LONG_MAX");
  else if(index >= vector->count)
    return E_NOT_FOUND;

  if(item)
    kmemcpy(item, vector_item(vector, index), vector->item_size);

  return E_OK;
}

int vector_set(vector_t * restrict vector, size_t index, const void * restrict item) {
  if(index == vector->count)
    return vector_insert(vector, index, item);

  kmemcpy(vector_item(vector, index), item, vector->item_size);
  return E_OK;
}

int vector_resize(vector_t *vector, size_t new_capacity) {
  void *new_data = krealloc(vector->data, new_capacity * vector->item_size, 0);

  new_capacity = new_capacity < MAX_CAPACITY ? new_capacity : MAX_CAPACITY;

  if(new_data) {
    vector->capacity = new_capacity;
    vector->data = new_data;

    return E_OK;
  }
  else
    return E_FAIL;
}

int vector_shrink_to_fit(vector_t *vector) {
  return vector->capacity > vector->count ?
    vector_resize(vector->data, vector->count) :
    E_OK;
}

int vector_insert(vector_t * restrict vector, size_t index, const void * restrict item) {
  if(index > vector->count)
    RET_MSG(E_FAIL, "Invalid index.");
  else if(vector->count + 1 > vector->capacity && IS_ERROR(vector_resize(vector, 2*vector->capacity)))
    RET_MSG(E_FAIL, "Unable to insert item to the end of the vector.");
  else {
    for(size_t i=vector->count; i > index; i--)
      kmemcpy(vector_item(vector, i), vector_item(vector, i-1), vector->item_size);

    kmemcpy(vector_item(vector, index), item, vector->item_size);

    vector->count++;

    return E_OK;
  }
}

int vector_remove(vector_t * restrict vector, size_t index, void * restrict item) {
  if(vector->count == 0)
    RET_MSG(E_FAIL, "Tried to removed item from empty vector.");
  else if(index > vector->count)
    RET_MSG(E_FAIL, "Tried to remove non-existent item.");
  else {
    if(item)
      kmemcpy(item, vector_item(vector, index), vector->item_size);

    for(size_t i=index+1; i < vector->count; i++)
      kmemcpy(vector_item(vector, i-1), vector_item(vector, i), vector->item_size);

    vector->count--;

    return E_OK;
  }
}

int vector_index_of(const vector_t *restrict vector, const void * restrict item) {
  for(size_t i=0; i < vector->count; i++) {
    if(memcmp(vector_item(vector, i), item, vector->item_size) == 0)
      return (int)i;
  }

  return E_NOT_FOUND;
}

int vector_rindex_of(const vector_t *restrict vector, const void * restrict item) {
  for(size_t i=vector->count; i > 0; i--) {
    if(memcmp(vector_item(vector, i-1), item, vector->item_size) == 0)
      return (int)(i-1);
  }

  return E_NOT_FOUND;
}
