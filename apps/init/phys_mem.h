#ifndef PHYS_MEM_H
#define PHYS_MEM_H

#include <types.h>

#define PAGE_SIZE       4096

extern char *physMap;
extern addr_t *freePageStack, *freePageStackTop;

addr_t allocPhysPage(void);
void freePhysPage(addr_t page);
int peek(addr_t phys, void *buffer, size_t len);
int poke(addr_t phys, void *buffer, size_t len);

#endif /* PHYS_MEM_H */
