#include <stdlib.h>
#include <kernel/algo.h>

void kqsort(void *base, size_t num_elem, size_t elem_size, int (*compare)(const void *x1, const void *x2)) {
  qsort(base, num_elem, elem_size, compare);
}
