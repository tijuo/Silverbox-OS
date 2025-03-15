#ifndef KERNEL_INIT_INIT_H
#define KERNEL_INIT_INIT_H

#include <stddef.h>
#include <kernel/debug.h>
#include <kernel/error.h>

#define DISC_CODE SECTION(".dtext")
#define DISC_DATA SECTION(".ddata")

extern int memcmp(const void* m1, const void* m2, size_t n);
extern int strncmp(const char*, const char*, size_t num);
extern size_t strlen(const char* s);
extern char* strstr(const char*, const char*);
extern char* strchr(const char*, int);

#endif /* KERNEL_INIT_INIT_H */
