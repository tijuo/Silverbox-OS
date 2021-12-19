#ifndef KERNEL_MEMORY_H

#define KERNEL_MEMORY_H

#include <util.h>
#include <kernel/mm.h>
#include <kernel/debug.h>
#include <stdint.h>

extern void* kmemcpy(void *restrict dest, const void *restrict src, size_t len);
extern void* kmemset(void *buffer, int c, size_t num);
extern size_t kstrlen(char *c);
extern char *kstrcpy(char *dest, const char *src);
extern char *kstrncpy(char *dest, const char *src, size_t len);

int clear_phys_page(paddr_t phys);
int initialize_root_pmap(paddr_t root_pmap);

NON_NULL_PARAMS int poke_virt(addr_t address, size_t len, void *buffer,
                             paddr_t root_pmap);
NON_NULL_PARAMS int peek_virt(addr_t address, size_t len, void *buffer,
                             paddr_t root_pmap);

/* This won't work with arrays that have been passed into functions or converted into pointers. */

#define ARR_SIZE(arr)     (sizeof(arr) / sizeof(arr[0]))
#define ARR_ITEM(base, i, size) ((void *)((uintptr_t)(base) + (i) * (size)))

#undef kalloca

#ifdef __GNUC__
#define kalloca(x) __builtin_alloca(x)
#else
#error "kalloca(x) needs to be defined."
#endif /* __GCC__ */

#endif /* KERNEL_MEMORY_H */
