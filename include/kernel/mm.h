#ifndef MM_H
#define MM_H

#include <types.h>
#include <kernel/multiboot.h>
#include <kernel/lowlevel.h>
#include <kernel/paging.h>
#include <kernel/memory.h>

/* FIXME: Changing any of these values may require changing
 the asm code */

#define KERNEL_TCB_START	((addr_t)&kTcbStart)
#define KERNEL_TCB_END		((addr_t)&kTcbEnd)
#define KERNEL_VSTART		((addr_t)&kVirtStart)
#define KERNEL_VEND		((addr_t)&kEnd)
#define KERNEL_PHYS_START	((addr_t)&kVirtLowMemStart)
#define KERNEL_START		((addr_t)&kPhysStart)
#define RESD_PHYSMEM	    	KERNEL_START

#define KVIRT_TO_PHYS(x)	((addr_t)(x) - KERNEL_PHYS_START)
#define KPHYS_TO_VIRT(x)	((addr_t)(x) + KERNEL_PHYS_START)

/* FIXME: Changing any of these values may require changing the
 asm code. */

#define KMAP_AREA               (0xFF800000u)
#define KMAP_AREA2		(0xFFC00000u)
#define IOAPIC_VADDR            KMAP_AREA2
#define LAPIC_VADDR             (IOAPIC_VADDR + 0x100000u)
#define TEMP_PAGE               (KMAP_AREA2 + 0x3FF000u)
#define INVALID_VADDR       	((addr_t)0xFFFFFFFF)
#define INVALID_ADDR        	((addr_t)0xFFFFFFFF)

#define INIT_SERVER_STACK_TOP	(ALIGN_DOWN((addr_t)KERNEL_VSTART, PAGE_TABLE_SIZE))
#define INIT_SERVER_STACK_SIZE   0x400000u
/** Aligns an address to the previous boundary (if not already aligned) */
#define ALIGN_DOWN(addr, boundary)	((addr_t)((addr) & ~((boundary) - 1) ))

#define IS_ALIGNED(addr, boundary)	(((addr) & ((boundary) - 1)) == 0)

/** Aligns an address to next boundary (even if it's already aligned) */

#define ALIGN_NEXT(addr, boundary)	(ALIGN_DOWN(addr, boundary) + boundary)

/// Aligns an address to the next boundary (if not already aligned)
#define ALIGN_UP(addr, boundary) \
({ \
  __typeof__ (boundary) _boundary=(boundary);  \
  __typeof__ (addr) _addr=(addr);              \
  (_addr == ALIGN_DOWN(_addr, _boundary) ?     \
    _addr : ALIGN_NEXT(_addr, _boundary)); \
})

#define ALIGN(addr, boundary)         ALIGN_UP(addr, boundary)

int initializeRootPmap(dword pmap);

NON_NULL_PARAMS int peek(uint64_t, void*, size_t);

NON_NULL_PARAMS int poke(uint64_t, void*, size_t);

NON_NULL_PARAMS HOT
int peekVirt(addr_t address, size_t len, void *buffer, uint32_t addrSpace);

NON_NULL_PARAMS HOT
int pokeVirt(addr_t address, size_t len, void *buffer, uint32_t addrSpace);

/**
 Can the kernel perform some memory access at some virtual address in a particular address space?

 @param addr The virtual address to be tested.
 @param pdir The physical address of the address space.
 @param isReadOnly true if a read-only access. false for write or read/write access.
 @return true if address is accessible. false, otherwise.
 **/

bool isAccessible(addr_t addr, uint32_t pdir, bool isReadOnly);

/**
 Can the kernel read data from some virtual address in a particular address space?

 @param addr The virtual address to be tested.
 @param pdir The physical address of the address space
 @return true if address is readable. false, otherwise.
 **/

static inline bool isReadable(addr_t addr, uint32_t pdir) {
  return isAccessible(addr, pdir, true);
}

/**
 Can the kernel write data to some virtual address in a particular address space?

 @param addr The virtual address to be tested.
 @param pdir The physical address of the address space
 @return true if address is writable. false, otherwise.
 **/

static inline bool isWritable(addr_t addr, uint32_t pdir) {
  return isAccessible(addr, pdir, false);
}

//extern void addGDTEntry( word, addr_t, uint32, uint32 );

#endif
