#ifndef MM_H
#define MM_H

#include <types.h>
#include <kernel/multiboot.h>
#include <kernel/lowlevel.h>
#include <kernel/paging.h>
#include <kernel/memory.h>

/* FIXME: Changing any of these values may require changing
   the asm code */

#define KERNEL_TCB_START	        ((addr_t)&kTcbStart)
#define KERNEL_VSTART       	    ((addr_t)&kVirtStart)
#define PHYSMEM_START       	    ((addr_t)&VPhysMemStart)
#define KERNEL_START        	    ((addr_t)&kPhysStart)
#define RESD_PHYSMEM	    	    KERNEL_START

#define PAGE_STACK                  ((addr_t)&kVirtPageStack)

#define KVIRT_TO_PHYS(x)	        ((x) - (KERNEL_VSTART-KERNEL_START))

#define KERNEL_HEAP_START           ((addr_t)0xD0000000)
#define KERNEL_HEAP_LIMIT           ((addr_t)0xF0000000)

/* FIXME: Changing any of these values may require changing the
   asm code. */

#define TEMP_PAGEADDR               ((addr_t)0xC0000)
#define LAPIC_VADDR                 (TEMP_PAGEADDR + PAGE_SIZE)
#define KERNEL_CLOCK		        (LAPIC_VADDR + PAGE_SIZE)

#define INVALID_VADDR       	    ((addr_t)0xFFFFFFFF)
#define INVALID_ADDR        	    ((addr_t)0xFFFFFFFF)

#define INIT_SERVER_STACK_TOP	    ((addr_t)KERNEL_TCB_START)

/** Aligns an address to the previous boundary (if not already aligned) */
#define ALIGN_DOWN(addr, boundary)       ((addr_t)( (addr) & ~((boundary) - 1) ))

#define IS_ALIGNED(addr, boundary)       (((addr) & ((boundary) - 1)) == 0)

/** Aligns an address to next boundary (even if it's already aligned) */

#define ALIGN_NEXT(addr, boundary)       (ALIGN_DOWN(addr, boundary) + boundary)

/// Aligns an address to the next boundary (if not already aligned)
#define ALIGN_UP(addr, boundary) \
({ \
  __typeof__ (boundary) _boundary=(boundary);  \
  __typeof__ (addr) _addr=(addr);              \
  (_addr == ALIGN_DOWN(_addr, _boundary) ?     \
    _addr : ALIGN_NEXT(_addr, _boundary)); \
})

#define ALIGN(addr, boundary)         ALIGN_UP(addr, boundary)

/// Temporarily maps a physical page into the current address space
#define mapTemp( phys )             kMapPage((addr_t)(TEMP_PAGEADDR), (phys), PAGING_RW)

/// Unmaps the temporary page
#define unmapTemp() 	            kUnmapPage((addr_t)(TEMP_PAGEADDR), NULL)

HOT(int readPmapEntry(paddr_t pbase, int entry, void *buffer));
HOT(int writePmapEntry(paddr_t pbase, int entry, void *buffer));

HOT(int kMapPage( addr_t virt, paddr_t phys, u32 flags ));
HOT(int mapPage( addr_t virt, paddr_t phys, u32 flags, paddr_t paddrSpace ));
HOT(int kUnmapPage( addr_t virt, paddr_t *phys ));
HOT(int kMapPageTable( addr_t virt, paddr_t phys, u32 flags ));
HOT(int kUnmapPageTable( addr_t virt, paddr_t *phys ));
HOT(addr_t unmapPage( addr_t virt, paddr_t addrSpace ));

int initializeRootPmap(dword pmap);

int peek( paddr_t, void *, size_t );
int poke( paddr_t, void *, size_t );

HOT(int peekVirt( addr_t address, size_t len, void *buffer, paddr_t paddrSpace ));
HOT(int pokeVirt( addr_t address, size_t len, void *buffer, paddr_t paddrSpace ));

int clearPhysPage( paddr_t phys );

bool isReadable( addr_t addr, paddr_t addrSpace );
bool isWritable( addr_t addr, paddr_t addrSpace );

void invalidateTlb(void);
void invalidatePage( addr_t virt );
dword getRootPageMap(void);

extern paddr_t *freePageStack;
extern paddr_t *freePageStackTop;

//extern void addGDTEntry( word, addr_t, uint32, uint32 );

paddr_t allocPageFrame(void);
void freePageFrame(paddr_t frame);

extern size_t pageTableSize;

#endif
