#ifndef PAGING_H
#define PAGING_H

#include <kernel/lowlevel.h>
#include <kernel/error.h>
#include <stdint.h>

#ifdef PAE
#define MAX_PHYS_MEMORY             0x10000000000ull
#endif /* PAE */

#define KERNEL_RECURSIVE_PDE        1023u

#define PAGE_SIZE		            0x1000u
#define LARGE_PAGE_SIZE   	    	0x400000u
#define PAE_LARGE_PAGE_SIZE	    	0x200000u
#define PAGE_TABLE_SIZE	  	    	0x400000u

#define PMAP_ENTRY_COUNT					(PAGE_SIZE / sizeof(pmap_entry_t))
#define PDE_ENTRY_COUNT						(PAGE_SIZE / sizeof(pde_t))
#define LARGE_PDE_ENTRY_COUNT			    (PAGE_SIZE / sizeof(large_pde_t))
#define PTE_ENTRY_COUNT						(PAGE_SIZE / sizeof(pte_t))

#if __GNUC__
#define PDE(recursive_index, vaddr)         ({ uintptr_t ndx = (recursive_index) & 0x3FFu; (pde_t *)((ndx << 22u) | (ndx << 12u) | (((uintptr_t)(vaddr)) >> 22u)); })
#else
#define PDE(recursive_index, vaddr)         (pde_t *)((((uintptr_t)(recursive_index)) << 22u) | (((uintptr_t)(recursive_index)) << 12u) | (((uintptr_t)(vaddr)) >> 22u))
#endif /* __GNUC__ */

#define PTE(recursive_index, vaddr)         (pte_t *)((((uintptr_t)(recursive_index)) << 22u) | (((uintptr_t)(vaddr)) >> 10u))

#define CURRENT_PDE(vaddr)                  PDE(KERNEL_RECURSIVE_PDE, vaddr)
#define CURRENT_PTE(vaddr)                  PTE(KERNEL_RECURSIVE_PDE, vaddr)

#define PDE_INDEX(a)		    			(((a) >> 22) & 0x3FFu)
#define PTE_INDEX(a)		    			(((a) >> 12) & 0x3FFu)
#define PAGE_OFFSET(a)		    		    ((a) & (PAGE_SIZE-1))
#define LARGE_PAGE_OFFSET(a)                ((a) & (LARGE_PAGE_SIZE-1))

#define NDX_TO_VADDR(pde, pte, offset)      ((((pde) & 0x3FFu) << 22) | (((pte) & 0x3FFu) << 12) | (((offset) & 0x3FFu) << 2))

#define PAGING_PRES		        		    (1ul << 0)
#define PAGING_RW		        			(1ul << 1)
#define PAGING_RO		        			0ul
#define PAGING_SUPERVISOR	    		    0ul
#define PAGING_USER		        		    (1ul << 2)
#define PAGING_PWT		        		    (1ul << 3)
#define PAGING_PCD		       			    (1ul << 4)
#define PAGING_ACCESSED		    		    (1ul << 5)
#define PAGING_DIRTY		    			(1ul << 6)
#define PAGING_4KB_PAGE		    		    0ul
#define PAGING_4MB_PAGE		    		    (1ul << 7)
#define PTE_PAT                             (1ul << 7)
#define PAGING_GLOBAL		    			(1u << 8)

#define PDE_PAT                             (1u << 12)

#define PAGING_ERR_PRES		    		    (1u << 0)
#define PAGING_ERR_READ		    		    0u
#define PAGING_ERR_WRITE	    		    (1u << 1)
#define PAGING_ERR_SUPERVISOR			    0u
#define PAGING_ERR_USER		    		    (1u << 2)


#define PMAP_FLAG_BITS          			12u
#define PTE_FLAG_BITS                       12u
#define PDE_FLAG_BITS                       12u
#define LARGE_PDE_FLAG_BITS                 13u

#define PBASE_SHIFT							12u
#define LARGE_PBASE_SHIFT					22u

#define PMAP_FLAG_MASK              	    ((1ul << PMAP_FLAG_BITS)-1)
#define PTE_FLAG_MASK               	    ((1ul << PMAP_FLAG_BITS)-1)
#define PDE_FLAG_MASK               	    ((1ul << PMAP_FLAG_BITS)-1)
#define LARGE_PDE_FLAG_MASK         	    ((1ul << PMAP_FLAG_BITS)-1)

#define PBASE_TO_PADDR(frame)              (paddr_t)((frame) << PBASE_SHIFT)
#define PBASE_TO_ADDR(frame)               ((addr_t)(frame) << PBASE_SHIFT)

#define ADDR_TO_PBASE(addr)                (pbase_t)((addr) >> PBASE_SHIFT)
#define ADDR_TO_LARGE_PBASE(addr)          (pbase_t)(ALIGN_DOWN(addr, LARGE_PAGE_SIZE) >> PBASE_SHIFT)

#define PADDR_TO_PBASE(addr)    			(pbase_t)((addr) >> PBASE_SHIFT)
#define PADDR_TO_LARGE_PBASE(addr)         (pbase_t)(ALIGN_DOWN(addr, (paddr_t)LARGE_PAGE_SIZE) >> PBASE_SHIFT)

#define PAGE_BASE_MASK		   				((paddr_t)(spaddr_t)(-PAGE_SIZE))

#define PTE_BASE(pte) 						((paddr_t)(pte).base << PTE_FLAG_BITS)
#define PDE_BASE(pde) 						((paddr_t)(pde).base << PDE_FLAG_BITS)

#define PADDR_TO_PDE_BASE(addr)				(pbase_t)((addr) >> PDE_FLAG_BITS)
#define PADDR_TO_PTE_BASE(addr)				(pbase_t)((addr) >> PDE_FLAG_BITS)

#define CR3_PWT                 	        (1ul << 3)
#define CR3_PCD                 	        (1ul << 4)

#define CR3_BASE_MASK           	        PAGE_BASE_MASK

typedef paddr_t     paging_table_entry_t;

struct CR3_Struct {
    union {
        struct {
            paging_table_entry_t _resd1 : 3;
            paging_table_entry_t pwt : 1;
            paging_table_entry_t pcd : 1;
            paging_table_entry_t _resd2 : 7;
            paging_table_entry_t base : 20;
        };

        paging_table_entry_t value;
    };
};

typedef struct CR3_Struct cr3_t;

_Static_assert(sizeof(cr3_t) == sizeof(paging_table_entry_t), "cr3_t should be 4 bytes");


/// Represents a 4 kB PTE in a page table.
/// Physical addresses for 4 kB pages are limited to 32 bits.
typedef struct {
    union {
        struct {
            paging_table_entry_t is_present : 1;
            paging_table_entry_t is_read_write : 1;
            paging_table_entry_t is_user : 1;
            paging_table_entry_t pwt : 1;
            paging_table_entry_t pcd : 1;
            paging_table_entry_t accessed : 1;
            paging_table_entry_t dirty : 1;
            paging_table_entry_t pat : 1;
            paging_table_entry_t global : 1;
            paging_table_entry_t was_allocated : 1; // Page is from a kernel allocator
            paging_table_entry_t available : 2;
            paging_table_entry_t base : 20;
        };

        paging_table_entry_t value;
    };
} pte_t;

_Static_assert(sizeof(pte_t) == sizeof(paging_table_entry_t), "pte_t should be 4 bytes");

/// Represents an x86 PDE in a page directory.
struct PageDirEntry {
    union {
        struct {
            uint32_t is_present : 1;
            uint32_t is_read_write : 1;
            uint32_t is_user : 1;
            uint32_t pwt : 1;
            uint32_t pcd : 1;
            uint32_t accessed : 1;
            uint32_t available2 : 1; // available for OS use
            uint32_t is_page_sized : 1;    // should be 0
            uint32_t available : 1; // available for OS use
            uint32_t was_allocated : 1; // Page table is from a kernel allocator
            uint32_t available3 : 2; // available for OS use
            uint32_t base : 20;
        };

        uint32_t value;
    };
};

typedef struct PageDirEntry pde_t;

_Static_assert(sizeof(pde_t) == sizeof(paging_table_entry_t), "pde_t should be 4 bytes");

/// Represents an x86 4MB PDE in a page directory.
/// Physical addresses for 4 MB pages are limited to 40 bits.
struct LargePageDirEntry {
    union {
        struct {
            uint32_t is_present : 1;
            uint32_t is_read_write : 1;
            uint32_t is_user : 1;
            uint32_t pwt : 1;
            uint32_t pcd : 1;
            uint32_t accessed : 1;
            uint32_t dirty : 1;
            uint32_t is_page_sized : 1;   // should be 1
            uint32_t global : 1;
            uint32_t was_allocated : 1; // Page is from a kernel allocator
            uint32_t available : 2; // available for OS use
            uint32_t pat : 1;
            uint32_t base_upper : 8;  // bits 32-39 of 4 MB frame
            uint32_t _resd : 1;
            uint32_t base_lower : 10; // bits 22-31 of 4 MB frame
        };

        uint32_t value;
    };
};

typedef struct LargePageDirEntry large_pde_t;

_Static_assert(sizeof(large_pde_t) == sizeof(paging_table_entry_t), "large_pde_t should be 4 bytes");

struct PageMapEntry {
    union {
        pde_t pde;
        pte_t pte;
        large_pde_t large_pde;
        paging_table_entry_t value;
    };
};

typedef struct PageMapEntry pmap_entry_t;

_Static_assert(sizeof(pmap_entry_t) == sizeof(paging_table_entry_t), "pmap_entry_t should be 4 bytes");

WARN_UNUSED unsigned int clear_phys_frames_with_start(addr_t virt, paddr_t phys, unsigned int count);
WARN_UNUSED unsigned int clear_phys_frames(paddr_t phys, unsigned int count);

/**
 Flushes the entire TLB by reloading the CR3 register.

 This must be done when switching to a new address space.
 If only a few entries need to be flushed, use invalidatePage().
 */
static inline void invalidate_tlb(void)
{
    set_cr3(get_cr3());
}

/**
 Flushes a single page from the TLB.

 Instead of flushing the entire TLB, only a single page is flushed.
 This is faster than reload_cr3() for flushing a few entries.
 */
static inline void invalidate_page(addr_t virt)
{
    __asm__ __volatile__("invlpg (%0)\n" :: "r"(virt) : "memory");
}

WARN_UNUSED static inline paddr_t get_root_page_map(void)
{
    return (paddr_t)(get_cr3() & CR3_BASE_MASK);
}

WARN_UNUSED unsigned int map_temp(addr_t virt, paddr_t base, unsigned int count);
WARN_UNUSED unsigned int unmap_temp(addr_t virt, unsigned int count);

WARN_UNUSED NON_NULL_PARAMS int read_pmap_entry(paddr_t pbase, unsigned int entry, pmap_entry_t *pmap_entry);
WARN_UNUSED int write_pmap_entry(paddr_t pbase, unsigned int entry, pmap_entry_t buffer);

/**
 Reads a page directory entry from an address space.

 @param entry_num The index of the PDE in the page directory
 @param page_dir The physical address of the page directory. If
 NULL_PADDR, then use the physical address of the current
 page directory in register CR3.
 @return The PDE that was read.
 */
WARN_UNUSED NON_NULL_PARAMS static inline int read_pde(pde_t *pde, unsigned int entry_num, paddr_t page_dir)
{
    return read_pmap_entry(page_dir, entry_num, (pmap_entry_t *)pde);
}

/**
 Writes a page directory entry to an address space.

 @param entry_num The index of the PDE in the page directory
 @param pde The PDE to be written.
 @param page_dir The physical address of the page directory. If
 NULL_PADDR, then use the physical address of the current
 page directory in register CR3.
 @return E_OK on success. E_FAIL on failure.
 */
WARN_UNUSED static inline int write_pde(unsigned int entry_num, pde_t pde, paddr_t page_dir)
{
    return write_pmap_entry(page_dir, entry_num, (pmap_entry_t){ .pde = pde });
}

/**
 Read a page table entry from a page table mapped in an address space.

 @param entry_num The page table entry number.
 @param page_table The physical address of the page table.
 @return The PTE that was read.
 */
WARN_UNUSED static inline int read_pte(pte_t *pte, unsigned int entry_num, paddr_t page_table)
{
    return read_pmap_entry(page_table, entry_num, (pmap_entry_t *)pte);
}

/**
 Write a page table entry to a page table mapped in an address space.

 @param entry_num The page table entry number.
 @param pte The PTE to be written.
 @param page_table The physical address of the page table.
 @return E_OK on success. E_FAIL on failure.
 */
WARN_UNUSED static inline int write_pte(unsigned int entry_num, pte_t pte, paddr_t page_table)
{
    return write_pmap_entry(page_table, entry_num, (pmap_entry_t){ .pte = pte });
}

WARN_UNUSED CONST static inline pbase_t get_pde_frame_number(pde_t pde)
{
    pmap_entry_t entry = {
      .pde = pde
    };

    return (
        entry.pde.is_page_sized ?
        ((pbase_t)entry.large_pde.base_lower << 10) | ((pbase_t)entry.large_pde.base_upper << 20) :
        (pbase_t)entry.pde.base);
}

NON_NULL_PARAMS
static inline void set_large_pde_base(large_pde_t* large_pde, paddr_t pframe)
{
    large_pde->base_lower = (uint32_t)((pframe >> 10) & 0x3FFu);
    large_pde->base_upper = (uint32_t)((pframe >> 20) & 0xFFu);
}

#endif /* PAGING_H */
