#include <kernel/algo.h>
#include <stdint.h>
#include <kernel/memory.h>

/**
  Perform a binary search on a *sorted* array of elements.

  @param key The particular item to be found. Assumed to be of
  size, elem_size.
  @param base A pointer to the first element in the array.
  @param num_elems The number of items in the array.
  @param elem_size The size of each item in bytes.
  @param cmp A comparison function that compares two elements.
  It should return a positive value if the first element preceeds
  the second, a negative value if the first element succeeds the
  second, and zero if the two elements have the same ordering.
  @return A pointer to the located element, if found. NULL, if key
  wasn't found in the array.
*/

void* kbsearch(const void *key, const void *base, size_t num_elems, size_t elem_size,
              int (*cmp)(const void *k, const void *d))
{
  if(num_elems == 0)
    return NULL;

  size_t middle = num_elems / 2;
  int cmp_result = cmp(key, ARR_ITEM(base, middle, elem_size));

  if(cmp_result == 0)
    return ARR_ITEM(base, middle, elem_size);
  else if(cmp_result > 0) {
    num_elems--;
    base = ARR_ITEM(base, middle+1, elem_size);
  }

  return kbsearch(key, base, num_elems / 2, elem_size, cmp);
}

/**
  Performs quicksort by sorting an array of elements in-place.

  @param base A pointer to the first element in the array.
  @param num_elem The number of items in the array.
  @param elem_size The size of each item in bytes.
  @param compare A comparison function that compares two elements.
  It should return a positive value if the first element preceeds
  the second, a negative value if the first element succeeds the
  second, and zero if the two elements have the same ordering.
*/

void kqsort(void *base, size_t num_elem, size_t elem_size, int (*compare)(const void *x1, const void *x2)) {
  size_t pivot = 0;

  if(num_elem < 2)
    return;

  for(size_t i=1; i < num_elem; i++) {
    /* If an item to the right of the pivot is less than or equal to the pivot,
    then shift the pivot and all array members between the pivot and the item to the
    right by one, and then place the item where the pivot used to be. */

    if(compare(ARR_ITEM(base, i, elem_size), ARR_ITEM(base, pivot, elem_size)) <= 0) {
      void *t = kalloca(elem_size);

      kmemcpy(t, ARR_ITEM(base, i, elem_size), elem_size);

      for(size_t j=i; j > pivot; j--)
        kmemcpy(ARR_ITEM(base, j, elem_size), ARR_ITEM(base, j-1, elem_size), elem_size);

      kmemcpy(ARR_ITEM(base, pivot, elem_size), t, elem_size);
      pivot++;
    }
  }

  kqsort(base, pivot, elem_size, compare);
  kqsort(ARR_ITEM(base, pivot+1, elem_size), num_elem-pivot-1, elem_size, compare);
}
