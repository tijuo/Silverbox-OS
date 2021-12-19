#ifndef KERNEL_ALGO_H
#define KERNEL_ALGO_H

#include <stddef.h>
#include <util.h>

NON_NULL_PARAMS PURE void* kbsearch(const void *key, const void *base, size_t num_elems, size_t elem_size,
              int (*cmp)(const void *k, const void *d));
NON_NULL_PARAMS void kqsort(void *base, size_t num_elem, size_t elem_size, int (*compare)(const void *x1, const void *x2));

#endif /* KERNEL_ALGO_H */
