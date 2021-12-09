#ifndef KERNEL_MEMORY_H

#define KERNEL_MEMORY_H

#include "../type.h"
#include <util.h>
#include <kernel/mm.h>
#include <kernel/debug.h>

NON_NULL_PARAMS RETURNS_NON_NULL
void* memcpy(void *restrict dest, const void *restrict src, size_t len);

NON_NULL_PARAMS RETURNS_NON_NULL
void* memset(void *buffer, int c, size_t num);

int clear_phys_page(paddr_t phys);
int initialize_root_pmap(paddr_t root_pmap);

NON_NULL_PARAMS int poke_virt(addr_t address, size_t len, void *buffer,
                             paddr_t root_pmap);
NON_NULL_PARAMS int peek_virt(addr_t address, size_t len, void *buffer,
                             paddr_t root_pmap);

/* This won't work with arrays that have been passed into functions or converted into pointers. */

#define ARRAY_SIZE(arr)     (sizeof(arr) / sizeof(arr[0]))

#endif /* KERNEL_MEMORY_H */
