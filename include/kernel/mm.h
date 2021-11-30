#ifndef MM_H
#define MM_H

#include <types.h>
#include <kernel/lowlevel.h>
#include <kernel/pae.h>

/* FIXME: Changing any of these values may require changing
 the asm code */

#define KERNEL_TCB_START	((addr_t)&ktcb_start)
#define KERNEL_TCB_END		((addr_t)&ktcb_end)
#define KERNEL_VSTART		((addr_t)&kvirt_start)
#define KERNEL_VEND			((addr_t)&kend)

/* The first 120 MiB of physical memory are mapped in kernel space. */

#define KERNEL_PHYS_START	((addr_t)&kvirt_low_mem_start)

#define INVALID_VADDR       ((addr_t)0xFFFFFFFFFFFFFFFFul)
#define INVALID_ADDR        ((addr_t)0xFFFFFFFFFFFFFFFFul)

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

int initialize_root_pmap(paddr_t root_pmap);

NON_NULL_PARAMS HOT
int peek_virt(addr_t address, size_t len, void *buffer, paddr_t addr_space);

NON_NULL_PARAMS HOT
int poke_virt(addr_t address, size_t len, void *buffer, paddr_t addr_space);

/**
 Can the kernel perform some memory access at some virtual address in a particular address space?

 @param addr The virtual address to be tested.
 @param root_pmap The physical address of the address space.
 @param is_read_only true if a read-only access. false for write or read/write access.
 @return true if address is accessible. false, otherwise.
 **/

bool is_accessible(addr_t addr, paddr_t root_pmap, bool is_read_only);

/**
 Can the kernel read data from some virtual address in a particular address space?

 @param addr The virtual address to be tested.
 @param root_pmap The physical address of the address space
 @return true if address is readable. false, otherwise.
 **/

static inline bool is_readable(addr_t addr, paddr_t root_pmap) {
  return is_accessible(addr, root_pmap, true);
}

/**
 Can the kernel write data to some virtual address in a particular address space?

 @param addr The virtual address to be tested.
 @param root_pmap The physical address of the address space
 @return true if address is writable. false, otherwise.
 **/

static inline bool is_writable(addr_t addr, paddr_t root_pmap) {
  return is_accessible(addr, root_pmap, false);
}

#if 0

#include <types.h>
#include <kernel/lowlevel.h>
#include <kernel/paging.h>

/* FIXME: Changing any of these values may require changing
 the asm code */

#define KERNEL_TCB_START	((addr_t)&ktcb_start)
#define KERNEL_TCB_END		((addr_t)&ktcb_end)
#define KERNEL_VSTART		((addr_t)&kvirt_start)
#define KERNEL_VEND			((addr_t)&kend)

/* The first 120 MiB of physical memory are mapped in kernel space. */

#define KERNEL_PHYS_START	((addr_t)&kvirt_low_mem_start)
#define KERNEL_START		((addr_t)&kPhysStart)
#define RESD_PHYSMEM	    KERNEL_START

/* FIXME: Changing any of these values may require changing the
 asm code. */

#define KMAP_AREA			(0xFF800000u)
#define KMAP_AREA2			(0xFFC00000u)
#define IOAPIC_VADDR		KMAP_AREA2
#define LAPIC_VADDR         (IOAPIC_VADDR + 0x100000u)
#define TEMP_PAGE			(KMAP_AREA2 + 0x3FF000u)
#define INVALID_VADDR       ((addr_t)0xFFFFFFFF)
#define INVALID_ADDR        ((addr_t)0xFFFFFFFF)

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

/**
 * Converts a kernel-mapped address to its physical address.
 *
 * @addr - The virtual address in kernel space. Must be >= KERNEL_PHYS_START and < KMAP_AREA.
 * @return The corresponding physical address.
 */

#ifdef DEBUG
#define KVIRT_TO_PHYS(addr) ({ \
  __typeof__ (addr) _addr=(addr); \
  assert(_addr >= KERNEL_PHYS_START); \
  assert(_addr < KMAP_AREA); \
  _addr - KERNEL_PHYS_START; \
})
#else
#define KVIRT_TO_PHYS(addr) ((addr_t)addr - KERNEL_PHYS_START)
#endif /* DEBUG */

/**
 * Converts a physical address into its kernel-mapped address.
 *
 * @addr - The physical address. Must be within the first 120 MiB of memory.
 * @return The corresponding kernel-space virtual address.
 */

#ifdef DEBUG
#define KPHYS_TO_VIRT(addr) ({ \
  __typeof__ (addr) _addr=(addr); \
  assert(_addr < (KMAP_AREA-KERNEL_PHYS_START)); \
  _addr + KERNEL_PHYS_START; \
})
#else
#define KPHYS_TO_VIRT(addr) ((addr_t)addr + KERNEL_PHYS_START)
#endif /* DEBUG */
int initialize_root_pmap(dword pmap);

NON_NULL_PARAMS int peek(uint64_t, void*, size_t);

NON_NULL_PARAMS int poke(uint64_t, void*, size_t);

NON_NULL_PARAMS HOT
int peek_virt(addr_t address, size_t len, void *buffer, paddr_t addr_space);

NON_NULL_PARAMS HOT
int poke_virt(addr_t address, size_t len, void *buffer, paddr_t addr_space);

/**
 Can the kernel perform some memory access at some virtual address in a particular address space?

 @param addr The virtual address to be tested.
 @param pdir The physical address of the address space.
 @param is_read_only true if a read-only access. false for write or read/write access.
 @return true if address is accessible. false, otherwise.
 **/

bool is_accessible(addr_t addr, paddr_t pdir, bool is_read_only);

/**
 Can the kernel read data from some virtual address in a particular address space?

 @param addr The virtual address to be tested.
 @param pdir The physical address of the address space
 @return true if address is readable. false, otherwise.
 **/

static inline bool is_readable(addr_t addr, paddr_t pdir) {
  return is_accessible(addr, pdir, true);
}

/**
 Can the kernel write data to some virtual address in a particular address space?

 @param addr The virtual address to be tested.
 @param pdir The physical address of the address space
 @return true if address is writable. false, otherwise.
 **/

static inline bool is_writable(addr_t addr, paddr_t root_pmap) {
  return is_accessible(addr, root_pmap, false);
}

#endif /* 0 */
#endif
