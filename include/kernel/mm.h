#ifndef MM_H
#define MM_H

#include <types.h>
#include <kernel/multiboot.h>
#include <kernel/lowlevel.h>
#include <kernel/paging.h>
#include <kernel/memory.h>

#define INVALID_PFRAME             (addr_t)0xFFFFFFFFu

/* FIXME: Changing any of these values may require changing
   the asm code */

#define KERNEL_TCB_START	        ((addr_t)&kTcbStart)
#define KERNEL_TCB_END		        ((addr_t)&kTcbEnd)
#define KERNEL_VSTART       	    ((addr_t)&kVirtStart)
#define KERNEL_VEND               ((addr_t)&kEnd)
#define PHYSMEM_START       	    ((addr_t)&VPhysMemStart)
#define KERNEL_START        	    ((addr_t)&kPhysStart)
#define RESD_PHYSMEM	    	    	KERNEL_START

#define PAGE_STACK                  ((addr_t)&kVirtPageStack)

#define KVIRT_TO_PHYS(x)	        ((addr_t)(x) + (KERNEL_START-KERNEL_VSTART))
#define KPHYS_TO_VIRT(x)					((addr_t)(x) - (KERNEL_START-KERNEL_VSTART))

#define PHYS_MAP_LIMIT              0x40000000u

#define MAX_PHYS_MEMORY							0x80000000u

/* FIXME: Changing any of these values may require changing the
   asm code. */

#define KMAP_AREA			        (0xFF000u)
#define TEMP_PAGEADDR		    	KMAP_AREA
#define LAPIC_VADDR                 (0x100000u)

#define INVALID_VADDR       	    ((addr_t)0xFFFFFFFF)
#define INVALID_ADDR        	    ((addr_t)0xFFFFFFFF)

#define INIT_SERVER_STACK_TOP	    ((addr_t)KERNEL_TCB_START)
#define INIT_SERVER_STACK_SIZE      0x400000u
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
#define mapTemp( phys )             kMapPage((addr_t)TEMP_PAGEADDR, (phys), PAGING_RW)

/// Unmaps the temporary page
#define unmapTemp() 	            kUnmapPage((addr_t)TEMP_PAGEADDR, NULL)

int initializeRootPmap(dword pmap);

NON_NULL_PARAMS int peek( paddr_t, void *, size_t );

NON_NULL_PARAMS int poke( paddr_t, void *, size_t );

NON_NULL_PARAMS HOT
int peekVirt( addr_t address, size_t len, void *buffer, paddr_t addrSpace );

NON_NULL_PARAMS HOT
int pokeVirt( addr_t address, size_t len, void *buffer, paddr_t addrSpace );

int clearPhysPage( paddr_t phys );

/**
 Can the kernel perform some memory access at some virtual address in a particular address space?

 @param addr The virtual address to be tested.
 @param pdir The physical address of the address space.
 @param isReadOnly true if a read-only access. false for write or read/write access.
 @return true if address is accessible. false, otherwise.
 **/

bool isAccessible(addr_t addr, paddr_t pdir, bool isReadOnly);

/**
 Can the kernel read data from some virtual address in a particular address space?

 @param addr The virtual address to be tested.
 @param pdir The physical address of the address space
 @return true if address is readable. false, otherwise.
 **/

static inline bool isReadable(addr_t addr, paddr_t pdir) {
	return isAccessible(addr, pdir, true);
}

/**
 Can the kernel write data to some virtual address in a particular address space?

 @param addr The virtual address to be tested.
 @param pdir The physical address of the address space
 @return true if address is writable. false, otherwise.
 **/

static inline bool isWritable(addr_t addr, paddr_t pdir) {
	return isAccessible(addr, pdir, false);
}

extern addr_t *freePageStack;
extern addr_t *freePageStackTop;

//extern void addGDTEntry( word, addr_t, uint32, uint32 );

addr_t allocPageFrame(void);
void freePageFrame(addr_t frame);

extern size_t pageTableSize;

#endif
