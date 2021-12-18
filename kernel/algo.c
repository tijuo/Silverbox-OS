#include <kernel/algo.h>

static void kqsort_partition(void *base, size_t num_elem, size_t elem_size, int (*compare)(const void *x1, const void *x2)) {
  if(num_elem <= 1)
    return;
}

/**
  Perform a binary search on an array of elements.

  @param key The particular item to be found. Assumed to be of
  size, elem_size.
  @param base A pointer to the first element in the array.
  @param num_elems The number of items in the array.
  @param elem_size The size of each item in bytes.
  @param cmp A comparison function that compares two elements.
  It should return a negative value if the first element preceeds
  the second, a positive value if the first element succeeds the
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
  int cmp_result = cmp(key, (void *)((uintptr_t)base + elem_size * middle);

  if(cmp_result == 0)
    return middle_key;
  else if(cmp_result > 0) {
    num_elems--;
    base = (void *)((uintptr_t)base + elem_size * (middle + 1));
  }

  return kbsearch(key, base, num_elems / 2, size, cmp);
}


void kqsort(void *base, size_t num_elem, size_t elem_size, int (*compare)(const void *x1, const void *x2)) {
  
}
