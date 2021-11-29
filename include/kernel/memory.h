#ifndef KERNEL_MEMORY_H
#define KERNEL_MEMORY_H

#include <types.h>
#include <util.h>
#include <kernel/mm.h>
#include <kernel/debug.h>

NON_NULL_PARAMS RETURNS_NON_NULL
void* memcpy(void *restrict dest, const void *restrict src, size_t len);

NON_NULL_PARAMS RETURNS_NON_NULL
void* memset(void *buffer, int c, size_t num);

/* This won't work with arrays that have been passed into functions or converted into pointers. */

#define ARRAY_SIZE(arr)     (sizeof(arr) / sizeof(arr[0]))

#endif /* KERNEL_MEMORY_H */
