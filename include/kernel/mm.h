#ifndef MM_H
#define MM_H

#include <types.h>
#include <kernel/multiboot.h>
#include <kernel/lowlevel.h>
#include <kernel/paging.h>

/* FIXME: Changing any of these values may require changing
 the asm code */

#define KERNEL_VSTART		((addr_t)&kvirt_start)
#define KERNEL_VEND			((addr_t)&kend)

 /* The 3 MiB of physical memory, above the 1 MiB mark, are mapped in kernel space. */

#define KERNEL_LOMEM_START	((addr_t)&kvirt_low_mem_start)
#define KERNEL_START		((addr_t)&kphys_start)
#define RESD_PHYSMEM	    KERNEL_START

/* FIXME: Changing any of these values may require changing the
 asm code. */

#define KERNEL_TCB_START    0xFF000000u         // 8 MiB (assuming TCBs are 128 bytes in size)
#define KERNEL_TCB_END      0xFF800000u
#define KERNEL_LOWER_MEM    KERNEL_TCB_END      // 1 MiB
#define KERNEL_STACKS       0xFF900000u         // 256 KiB + 256 KiB (guard pages)
#define KERNEL_STACKS_TOP   0xFF980000u
#define IOAPIC_VADDR        KERNEL_STACKS_TOP   // 64 KiB
#define LAPIC_VADDR         0xFF990000u         // 4 KiB
#define KERNEL_TEMP_START   0xFF991000u         // 4 KiB
#define KERNEL_TEMP_END     0xFFC00000u

/*
    These are used for temporarily accessing pages, page directories, and page tables
    from the current address space.
*/

#define INIT_SERVER_STACK_TOP	(ALIGN_DOWN((addr_t)KERNEL_VSTART, PAGE_TABLE_SIZE))
#define INIT_SERVER_STACK_SIZE   0x400000u

 /** Aligns an address to the previous boundary (if not already aligned) */
#define ALIGN_DOWN(addr, boundary) ((addr_t)((addr) & ~((boundary) - 1) ))

_Static_assert(ALIGN_DOWN(0xFF10, 4096) == 0xF000);

#define IS_ALIGNED(addr, boundary)	(((addr) & ((boundary) - 1)) == 0)

/** Aligns an address to next boundary (even if it's already aligned) */
#define ALIGN_NEXT(addr, boundary)	(ALIGN_DOWN(addr, boundary) + boundary)

_Static_assert(ALIGN_NEXT(0x100000u, 4096) == ALIGN_DOWN(0x100000u, 4096) + 4096);

// Aligns an address to the next boundary (if not already aligned)

#ifdef __GNUC__
#define ALIGN_UP(addr, boundary) \
  ({ \
    __typeof__ (boundary) _boundary=(boundary);  \
    __typeof__ (addr) _addr=(addr);              \
    (_addr == ALIGN_DOWN(_addr, _boundary) ?     \
      _addr : ALIGN_NEXT(_addr, _boundary)); \
  })
#else
#define ALIGN_UP(addr, boundary) (addr == ALIGN_DOWN(addr, boundary) ? addr : ALIGN_NEXT(addr, boundary))
#endif /* __GNUC__ */

#define ALIGN(addr, boundary)         ALIGN_UP(addr, boundary)


/**
 * Converts a virtual address within the low memory area to its physical address.
 *
 * @addr - The virtual address in low memory area.
 * @return The corresponding physical address.
 */
#define VIRT_TO_PHYS(addr) ((paddr_t)((addr_t)(addr)- (addr_t)KERNEL_LOMEM_START))

 /**
  * Converts a physical address into its kernel-mapped address.
  *
  * @addr - The physical address. Must be within the first 120 MiB of memory.
  * @return The corresponding kernel-space virtual address.
  */

#define PHYS_TO_VIRT(addr) ((addr_t)addr + KERNEL_LOMEM_START)

WARN_UNUSED int initialize_root_pmap(paddr_t pmap);

WARN_UNUSED NON_NULL_PARAMS int peek(paddr_t, void*, size_t);

WARN_UNUSED NON_NULL_PARAMS int poke(paddr_t, void*, size_t);

NON_NULL_PARAMS HOT
WARN_UNUSED int peek_virt(addr_t address, size_t len, void* buffer, paddr_t addr_space);

NON_NULL_PARAMS HOT
WARN_UNUSED int poke_virt(addr_t address, size_t len, void* buffer, paddr_t addr_space);

WARN_UNUSED int is_accessible(addr_t addr, paddr_t pdir, bool is_read_only);

/**
 Can the kernel read data from some virtual address in a particular address space?

Temporarily maps memory.

 @param addr The virtual address to be tested.
 @param pdir The physical address of the address space
 @return 1 if the address is readable. 0, if the address is not readable. `E_FAIL`, on failure.

 **/
WARN_UNUSED static inline int is_readable(addr_t addr, paddr_t pdir)
{
    return is_accessible(addr, pdir, true);
}

/**
 Can the kernel write data to some virtual address in a particular address space?

Temporarily maps memory.

 @param addr The virtual address to be tested.
 @param pdir The physical address of the address space
 @return 1 if the address is writable. 0, if the address is not writable. `E_FAIL`, on failure.
 **/
WARN_UNUSED static inline int is_writable(addr_t addr, paddr_t pdir)
{
    return is_accessible(addr, pdir, false);
}

#endif
