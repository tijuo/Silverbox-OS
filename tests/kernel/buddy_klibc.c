#include <stdlib.h>
#include <kernel/algo.h>
#include <string.h>

void kqsort(void *base, size_t num_elem, size_t elem_size, int (*cmp)(const void *x, const void *y)) {
  qsort(base, num_elem, elem_size, cmp);
}

void kmemset(void *base, int value, size_t len) {
  memset(base, value, len);
}
