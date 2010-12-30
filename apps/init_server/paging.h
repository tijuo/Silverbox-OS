#ifndef PAGING_H
#define PAGING_H

#include "addr_space.h"

#define PAGE_SIZE       4096
#define PTABLE_SIZE     4194304

#define TEMP_PTABLE	0xE0000000
#define STACK_TABLE	0xE0000000
#define TEMP_PAGE	0x100000

/* FIXME: These should be passed to the init server by the kernel. */
#define KPHYS_START	0x1000000
#define KVIRT_START	0xFF400000

int _mapMem( void *phys, void *virt, int pages, int flags, struct AddrSpace *aSpace );
void *_unmapMem( void *virt, struct AddrSpace *aSpace );
int mapMemRange( void *virt, int pages );
void clearPage( void *page );
void setPage( void *page, char data );

#endif
