#ifndef PAGING_H
#define PAGING_H

#include <kernel/lowlevel.h>
#include <kernel/error.h>
#include <stdint.h>

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
#define PAGE_OFFSET(a)		    		((a) & 0xFFFu)

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

#define PFRAME_TO_ADDR(frame)   			(addr_t)((frame) << PFRAME_BITS)
#define ADDR_TO_PFRAME(addr)    			(unsigned int)(((uintptr_t)(addr)) >> PFRAME_BITS)

#define PAGE_BASE_MASK		   					0xFFFFF000u

#define PTE_BASE(pte) 								(paddr_t)((pte).base << PTE_FLAG_BITS)
#define PDE_BASE(pde) 								(paddr_t)((pde).base << PDE_FLAG_BITS)

#define PADDR_TO_PDE_BASE(addr)				(uint32_t)((addr) >> PDE_FLAG_BITS)
#define PADDR_TO_PTE_BASE(addr)				(uint32_t)((addr) >> PDE_FLAG_BITS)

#define LARGE_PDE_BASE(pde) ({\
	large_pde_t _pde=(pde); \
	((paddr_t)(_pde.baseLower) << 22) /*| ((paddr_t)(_pde->baseUpper) << 32)*/;\
})

#define CR3_PWT                 	(1u << 3)
#define CR3_PCD                 	(1u << 4)

#define CR3_BASE_MASK           	0xFFFFF000u

struct CR3_Struct {
  union {
    struct {
      uint32_t _resd1 :3;
      uint32_t pwt :1;
      uint32_t pcd :1;
      uint32_t _resd2 :7;
      uint32_t base :20;
    };

    uint32_t value;
  };
};

typedef struct CR3_Struct cr3_t;

/// Represents a 4 kB PTE in a page table.
/// Physical addresses for 4 kB pages are limited to 32 bits.

struct PageTableEntry {
  union {
    struct {
      uint32_t isPresent :1;
      uint32_t isReadWrite :1;
      uint32_t isUser :1;
      uint32_t pwt :1;
      uint32_t pcd :1;
      uint32_t accessed :1;
      uint32_t dirty :1;
      uint32_t pat :1;
      uint32_t global :1;
      uint32_t available :3;
      uint32_t base :20;
    };

    uint32_t value;
  };
};

typedef struct PageTableEntry pte_t;

/// Represents an x86 PDE in a page directory.

struct PageDirEntry {
  union {
    struct {
      uint32_t isPresent :1;
      uint32_t isReadWrite :1;
      uint32_t isUser :1;
      uint32_t pwt :1;
      uint32_t pcd :1;
      uint32_t accessed :1;
      uint32_t available2 :1;
      uint32_t isLargePage :1;    // should be 0
      uint32_t available :4;
      uint32_t base :20;
    };

    uint32_t value;
  };
};

typedef struct PageDirEntry pde_t;

/// Represents an x86 4MB PDE in a page directory.
/// Physical addresses for 4 MB pages are limited to 40 bits.

struct LargePageDirEntry {
  union {
    struct {
      uint32_t isPresent :1;
      uint32_t isReadWrite :1;
      uint32_t usPriv :1;
      uint32_t pwt :1;
      uint32_t pcd :1;
      uint32_t accessed :1;
      uint32_t dirty :1;
      uint32_t isLargePage :1;   // should be 1
      uint32_t global :1;
      uint32_t available :3;
      uint32_t pat :1;
      uint32_t baseUpper :8;  // bits 32-39 of 4 MB frame
      uint32_t _resd :1;
      uint32_t baseLower :10; // bits 22-31 of 4 MB frame
    };

    uint32_t value;
  };
};

typedef struct LargePageDirEntry large_pde_t;

struct PageMapEntry {
  union {
    pde_t pde;
    pte_t pte;
    large_pde_t largePde;
    uint32_t value;
  };
};

typedef struct PageMapEntry pmap_entry_t;

HOT pmap_entry_t readPmapEntry(uint32_t pbase, unsigned int entry);
HOT int writePmapEntry(uint32_t pbase, unsigned int entry,
                       pmap_entry_t pmapEntry);

int kMapPage(addr_t virt, paddr_t phys, u32 flags);
int mapPage(addr_t virt, paddr_t phys, u32 flags, uint32_t addrSpace);
int kUnmapPage(addr_t virt, paddr_t *phys);
int kMapPageTable(addr_t virt, paddr_t phys, u32 flags);
int kUnmapPageTable(addr_t virt, paddr_t *phys);
addr_t unmapPage(addr_t virt, uint32_t addrSpace);

/**
 Flushes the entire TLB by reloading the CR3 register.

 This must be done when switching to a new address space.
 If only a few entries need to be flushed, use invalidatePage().
 */

static inline void invalidateTlb(void) {
  setCR3(getCR3());
}

/**
 Flushes a single page from the TLB.

 Instead of flushing the entire TLB, only a single page is flushed.
 This is faster than reload_cr3() for flushing a few entries.
 */

static inline void invalidatePage(addr_t virt) {
  __asm__ __volatile__( "invlpg (%0)\n" :: "r"( virt ) : "memory" );
}

static inline paddr_t getRootPageMap(void) {
  return (paddr_t)(getCR3() & CR3_BASE_MASK);
}

/**
 Reads a page directory entry from an address space.

 @param entryNum The index of the PDE in the page directory
 @param pageDir The physical address of the page directory. If
 NULL_PADDR, then use the physical address of the current
 page directory in register CR3.
 @return The PDE that was read.
 */

static inline pde_t readPDE(unsigned int entryNum, paddr_t pageDir) {
  return readPmapEntry(pageDir, entryNum).pde;
}

/**
 Writes a page directory entry to an address space.

 @param entryNum The index of the PDE in the page directory
 @param pde The PDE to be written.
 @param pageDir The physical address of the page directory. If
 NULL_PADDR, then use the physical address of the current
 page directory in register CR3.
 @return E_OK on success. E_FAIL on failure.
 */

static inline int writePDE(unsigned int entryNum, pde_t pde, paddr_t pageDir) {
  pmap_entry_t pmapEntry = {
    .pde = pde
  };
  return writePmapEntry(pageDir, entryNum, pmapEntry);
}

/**
 Read a page table entry from a page table mapped in an address space.

 @param entryNum The page table entry number.
 @param pageTable The physical address of the page table.
 @return The PTE that was read.
 */

static inline pte_t readPTE(unsigned int entryNum, paddr_t pageTable) {
  return readPmapEntry(pageTable, entryNum).pte;
}

/**
 Write a page table entry to a page table mapped in an address space.

 @param entryNum The page table entry number.
 @param pte The PTE to be written.
 @param pageTable The physical address of the page table.
 @return E_OK on success. E_FAIL on failure.
 */

static inline int writePTE(unsigned int entryNum, pte_t pte, paddr_t pageTable)
{
  pmap_entry_t pmapEntry = {
    .pte = pte
  };
  return writePmapEntry(pageTable, entryNum, pmapEntry);
}

CONST static inline uint32_t getPdeFrameNumber(pde_t pde) {
  pmap_entry_t entry = {
    .pde = pde
  };

  return (
      entry.pde.isLargePage ?
          entry.largePde.baseLower | (entry.largePde.baseUpper << 10) :
          entry.pde.base);
}

CONST static inline paddr_t getPdeBase(pde_t pde) {
  return getPdeFrameNumber(pde)
      << (pde.isLargePage ? LARGE_PFRAME_BITS : PFRAME_BITS);
}

NON_NULL_PARAMS
static inline void setLargePdeBase(large_pde_t *largePde, paddr_t paddr)
{
  largePde->baseLower = (uint32_t)((paddr >> LARGE_PFRAME_BITS) & 0x3FFu);
  largePde->baseUpper = 0; /*(uint32_t)((_paddr >> 32) & 0xFF)*/
}

#endif /* PAGING_H */
