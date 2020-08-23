#ifndef KMALLOC_H
#define KMALLOC_H

#include <types.h>

void kfree(void *p, size_t size);
void *kmalloc(size_t size);

#endif /* KMALLOC_H */