#ifndef KERNEL_MEMORY_H

#define KERNEL_MEMORY_H

#include <util.h>
#include <kernel/mm.h>
#include <kernel/debug.h>
#include <stdint.h>

extern void* kmemcpy(void *restrict dest, const void *restrict src, size_t len);
extern void* kmemset(void *buffer, int c, size_t num);
extern PURE size_t kstrlen(char *c);
extern char *kstrcpy(char *dest, const char *src);
extern char *kstrncpy(char *dest, const char *src, size_t len);
extern PURE int kstrcmp(const char *s1, const char *s2);
extern PURE int kstrncmp(const char *s1, const char *s2, size_t len);
extern PURE int kmemcmp(const void *m1, const void *m2, size_t len);

int clear_phys_page(paddr_t phys);

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
