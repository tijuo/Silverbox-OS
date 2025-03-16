#ifndef PAGING_H
#define PAGING_H

#include <kernel/lowlevel.h>
#include <kernel/error.h>
#include <stdint.h>

#define MAX_PHYS_MEMORY           0x10000000000ull

#define PAGE_SIZE		            	0x1000u
#define LARGE_PAGE_SIZE   	    	0x400000u
#define PAE_LARGE_PAGE_SIZE	    	0x200000u
#define PAGE_TABLE_SIZE	  	    	0x400000u

#define PMAP_ENTRY_COUNT					(PAGE_SIZE / sizeof(pmap_entry_t))
#define PDE_ENTRY_COUNT						(PAGE_SIZE / sizeof(pde_t))
#define LARGE_PDE_ENTRY_COUNT			(PAGE_SIZE / sizeof(large_pde_t))
#define PTE_ENTRY_COUNT						(PAGE_SIZE / sizeof(pte_t))

#define VIRT_TO_PHYS( addr )			(addr_t)( (addr) + &kVirtToPhys )
#define PHYS_TO_VIRT( addr )			(addr_t)( (addr) + &kPhysToVirt )

#define PDE_INDEX(a)		    			(((a) >> 22) & 0x3FFu)
#define PTE_INDEX(a)		    			(((a) >> 12) & 0x3FFu)
#define PAGE_OFFSET(a)		    		((a) & (PAGE_SIZE-1))
#define LARGE_PAGE_OFFSET(a)      ((a) & (LARGE_PAGE_SIZE-1))

#define NDX_TO_VADDR(pde, pte, offset)  ((((pde) & 0x3FFu) << 22) | (((pte) & 0x3FFu) << 12) | ((offset) & 0xFFFu))

#define PAGING_PRES		        		(1u << 0)
#define PAGING_RW		        			(1u << 1)
#define PAGING_RO		        			0u
#define PAGING_SUPERVISOR	    		0u
#define PAGING_USER		        		(1u << 2)
#define PAGING_PWT		        		(1u << 3)
#define PAGING_PCD		       			(1u << 4)
#define PAGING_ACCESSED		    		(1u << 5)
#define PAGING_DIRTY		    			(1u << 6)
#define PAGING_4KB_PAGE		    		0u
#define PAGING_4MB_PAGE		    		(1u << 7)
#define PTE_PAT                   (1u << 7)
#define PAGING_GLOBAL		    			(1u << 8)

#define PDE_PAT                   (1u << 12)

#define PAGING_ERR_PRES		    		(1u << 0)
#define PAGING_ERR_READ		    		0u
#define PAGING_ERR_WRITE	    		(1u << 1)
#define PAGING_ERR_SUPERVISOR			0u
#define PAGING_ERR_USER		    		(1u << 2)

#define PMAP_FLAG_BITS          				12u
#define PTE_FLAG_BITS                   12u
#define PDE_FLAG_BITS                   12u
#define LARGE_PDE_FLAG_BITS             13u

#define PFRAME_BITS											12u
#define LARGE_PFRAME_BITS								22u

#define PMAP_FLAG_MASK              	((1u << PMAP_FLAG_BITS)-1)
#define PTE_FLAG_MASK               	((1u << PMAP_FLAG_BITS)-1)
#define PDE_FLAG_MASK               	((1u << PMAP_FLAG_BITS)-1)
#define LARGE_PDE_FLAG_MASK         	((1u << PMAP_FLAG_BITS)-1)

#define PFRAME_TO_PADDR(frame)        ((uint64_t)(frame) << PFRAME_BITS)
#define PFRAME_TO_ADDR(frame)         ((addr_t)(frame) << PFRAME_BITS)

#define ADDR_TO_PFRAME(addr)         (pframe_t)((addr) >> PFRAME_BITS)
#define ADDR_TO_LARGE_PFRAME(addr)    (pframe_t)(ALIGN_DOWN(addr, LARGE_PAGE_SIZE) >> PFRAME_BITS)

#define PADDR_TO_PFRAME(addr)    			(pframe_t)((addr) >> PFRAME_BITS)
#define PADDR_TO_LARGE_PFRAME(addr)    (pframe_t)(ALIGN_DOWN(addr, (uint64_t)LARGE_PAGE_SIZE) >> PFRAME_BITS)

#define PAGE_BASE_MASK		   					0xFFFFF000u

#define PTE_BASE(pte) 								((pte).base << PTE_FLAG_BITS)
#define PDE_BASE(pde) 								((pde).base << PDE_FLAG_BITS)

#define PADDR_TO_PDE_BASE(addr)				(uint32_t)((addr) >> PDE_FLAG_BITS)
#define PADDR_TO_PTE_BASE(addr)				(uint32_t)((addr) >> PDE_FLAG_BITS)

#define CR3_PWT                 	(1u << 3)
#define CR3_PCD                 	(1u << 4)

#define CR3_BASE_MASK           	0xFFFFF000u

struct CR3_Struct {
    union {
        struct {
            uint32_t _resd1 : 3;
            uint32_t pwt : 1;
            uint32_t pcd : 1;
            uint32_t _resd2 : 7;
            uint32_t base : 20;
        };

        uint32_t value;
    };
};

typedef struct CR3_Struct cr3_t;

_Static_assert(sizeof(cr3_t) == 4, "cr3_t should be 4 bytes");


/// Represents a 4 kB PTE in a page table.
/// Physical addresses for 4 kB pages are limited to 32 bits.
struct PageTableEntry {
    union {
        struct {
            uint32_t is_present : 1;
            uint32_t is_read_write : 1;
            uint32_t is_user : 1;
            uint32_t pwt : 1;
            uint32_t pcd : 1;
            uint32_t accessed : 1;
            uint32_t dirty : 1;
            uint32_t pat : 1;
            uint32_t global : 1;
            uint32_t available : 3;
            uint32_t base : 20;
        };

        uint32_t value;
    };
};

typedef struct PageTableEntry pte_t;

_Static_assert(sizeof(pte_t) == 4, "pte_t should be 4 bytes");

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
            uint32_t available2 : 1;
            uint32_t is_large_page : 1;    // should be 0
            uint32_t available : 4;
            uint32_t base : 20;
        };

        uint32_t value;
    };
};

typedef struct PageDirEntry pde_t;

_Static_assert(sizeof(pde_t) == 4, "pde_t should be 4 bytes");

/// Represents an x86 4MB PDE in a page directory.
/// Physical addresses for 4 MB pages are limited to 40 bits.
struct LargePageDirEntry {
    union {
        struct {
            uint32_t is_present : 1;
            uint32_t is_read_write : 1;
            uint32_t usPriv : 1;
            uint32_t pwt : 1;
            uint32_t pcd : 1;
            uint32_t accessed : 1;
            uint32_t dirty : 1;
            uint32_t is_large_page : 1;   // should be 1
            uint32_t global : 1;
            uint32_t available : 3;
            uint32_t pat : 1;
            uint32_t base_upper : 8;  // bits 32-39 of 4 MB frame
            uint32_t _resd : 1;
            uint32_t base_lower : 10; // bits 22-31 of 4 MB frame
        };

        uint32_t value;
    };
};

typedef struct LargePageDirEntry large_pde_t;

_Static_assert(sizeof(large_pde_t) == 4, "large_pde_t should be 4 bytes");

struct PageMapEntry {
    union {
        pde_t pde;
        pte_t pte;
        large_pde_t large_pde;
        uint32_t value;
    };
};

typedef struct PageMapEntry pmap_entry_t;

_Static_assert(sizeof(pmap_entry_t) == 4, "pmap_entry_t should be 4 bytes");

pmap_entry_t read_pmap_entry(uint32_t pbase, unsigned int entry);
int write_pmap_entry(uint32_t pbase, unsigned int entry,
    pmap_entry_t pmap_entry);
int map_large_frame(uint64_t phys, pmap_entry_t* pmap_entry);
int unmap_large_frame(pmap_entry_t pmap_entry);
int clear_phys_page(uint32_t phys);

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

static inline uint32_t get_root_page_map(void)
{
    return (uint32_t)(get_cr3() & CR3_BASE_MASK);
}

/**
 Reads a page directory entry from an address space.

 @param entry_num The index of the PDE in the page directory
 @param page_dir The physical address of the page directory. If
 NULL_PADDR, then use the physical address of the current
 page directory in register CR3.
 @return The PDE that was read.
 */
static inline pde_t read_pde(unsigned int entry_num, uint32_t page_dir)
{
    return read_pmap_entry(page_dir, entry_num).pde;
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
static inline int write_pde(unsigned int entry_num, pde_t pde, uint32_t page_dir)
{
    pmap_entry_t pmap_entry = {
      .pde = pde
    };
    return write_pmap_entry(page_dir, entry_num, pmap_entry);
}

/**
 Read a page table entry from a page table mapped in an address space.

 @param entry_num The page table entry number.
 @param page_table The physical address of the page table.
 @return The PTE that was read.
 */
static inline pte_t read_pte(unsigned int entry_num, uint32_t page_table)
{
    return read_pmap_entry(page_table, entry_num).pte;
}

/**
 Write a page table entry to a page table mapped in an address space.

 @param entry_num The page table entry number.
 @param pte The PTE to be written.
 @param page_table The physical address of the page table.
 @return E_OK on success. E_FAIL on failure.
 */
static inline int write_pte(unsigned int entry_num, pte_t pte, uint32_t page_table)
{
    pmap_entry_t pmapEntry = {
      .pte = pte
    };
    return write_pmap_entry(page_table, entry_num, pmapEntry);
}

CONST static inline pframe_t get_pde_frame_number(pde_t pde)
{
    pmap_entry_t entry = {
      .pde = pde
    };

    return (
        entry.pde.is_large_page ?
        (entry.large_pde.base_lower << 10) | (entry.large_pde.base_upper << 20) :
        entry.pde.base);
}

NON_NULL_PARAMS
static inline void set_large_pde_base(large_pde_t* large_pde, pframe_t pframe)
{
    large_pde->base_lower = (uint32_t)((pframe >> 10) & 0x3FFu);
    large_pde->base_upper = (uint32_t)((pframe >> 20) & 0xFFu);
}

#endif /* PAGING_H */
