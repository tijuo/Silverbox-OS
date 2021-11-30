#ifndef KERNEL_PAE_H
#define KERNEL_PAE_H

/*
 * From Intel® 64 and IA-32 Architectures Software Developer’s Manual, Vol. 3A:

 Paging behavior is controlled by the following control bits:
 • The WP and PG flags in control register CR0 (bit 16 and bit 31, respectively).
 • The PSE, PAE, PGE, LA57, PCIDE, SMEP, SMAP, PKE, CET, and PKS flags in control register CR4
 (bit 4, bit 5, bit 7, bit 12, bit 17, bit 20, bit 21, bit 22, bit 23, and bit 24, respectively).
 • The LME and NXE flags in the IA32_EFER MSR (bit 8 and bit 11, respectively).
 • The AC flag in the EFLAGS register (bit 18).

 Software enables paging by using the MOV to CR0 instruction to set CR0.PG.
 Before doing so, software should ensure that control register CR3 contains the
 physical address of the first paging structure that the processor will use for
 linear-address translation (see Section 4.2) and that that structure is initialized
 as desired.
 */

#include <stdint.h>

#define PAGE_BITS		12u
#define PAGE_SIZE		(1ul << PAGE_BITS)

/*
 * CR3 contains the physical address of the PML4 table.
 * * Page directories and page tables must be aligned to a 4-KiB boundary.
 * * 2 MB pages must be aligned to a 2-MiB boundary.
 * * 1 GiB pages must be aligned to a 1-GiB boundary.
 *
 * Only PDPTEs, PDEs, and PTEs that point to pages have dirty, global, and PAT bits.
 */

#define PAE_PRESENT         (1ul << 0)
#define PAE_RW              (1ul << 1)
#define PAE_US              (1ul << 2)
#define PAE_PWT             (1ul << 3)
#define PAE_PCD             (1ul << 4)
#define PAE_ACCESSED        (1ul << 5)
#define PAE_DIRTY           (1ul << 6)
#define PAE_PTE_PAT			(1ul << 7)
#define PAE_PS				(1ul << 7)
#define PAE_GLOBAL          (1ul << 8)
#define PAE_PDE_PAT         (1ul << 12)
#define PAE_PDPTE_PAT		(1ul << 12)
#define PAE_XD              (1ul << 63)	// EFER.NXE must be set for this bit to be valid

#define PAE_HUGE_PAGE_SIZE	(PAE_LARGE_PAGE_SIZE * PAE_PMAP_ENTRIES)
#define PAE_LARGE_PAGE_SIZE	(PAGE_SIZE * PAE_PMAP_ENTRIES)
#define MAX_PHYS_ADDR_BITS  52				// this value needs to be determined from cpuid

#define PAE_ENTRY_BITS		9u

#define PAE_PMAP_ENTRIES	(1u << PAE_ENTRY_BITS)

#define PML4_INDEX(addr)	(size_t)(((addr) >> (PAGE_BITS + 3*PAGE_ENTRY_BITS)) % PAE_PMAP_ENTRIES)
#define PDPTE_INDEX(addr)   (size_t)(((addr) >> (PAGE_BITS + 2*PAGE_ENTRY_BITS)) % PAE_PMAP_ENTRIES)
#define PDE_INDEX(addr)     (size_t)(((addr) >> (PAGE_BITS + PAGE_ENTRY_BITS)) % PAE_PMAP_ENTRIES)
#define PTE_INDEX(addr)     (size_t)(((addr) >> PAGE_BITS) % PAE_PMAP_ENTRIES)
#define PAGE_OFFSET(addr)   (size_t)(((addr) % PAGE_SIZE)

#define INDEX_TO_ADDR(pml4e, pdpte, pde, pte, offset)	\
  (addr_t)((((pml4e) % PAE_PMAP_ENTRIES) << (PAGE_BITS + 3*PAE_ENTRY_BITS)) \
      | (((pdpte) % PAE_PMAP_ENTRIES) << (PAGE_BITS + 2*PAE_ENTRY_BITS)) \
	  | (((pde) % PAE_PMAP_ENTRIES) << (PAGE_BITS + PAE_ENTRY_BITS)) \
	  | (((pte) % PAE_PMAP_ENTRIES) << PAGE_BITS) | (offset % PAGE_SIZE))

#define CR3_BASE_MASK		0xFFFFFFFFFFFFF000ul

typedef uint64_t pmap_entry_t;

static inline void set_pte_base(pmap_entry_t *entry, paddr_t addr) {
  *entry |= addr & ~(PAGE_SIZE-1) & (max_phys_addr-1);
}

static inline void set_pde_base(pmap_entry_t *entry, paddr_t addr) {
  *entry |= addr & ~(PAGE_SIZE-1) & (max_phys_addr-1);
}

static inline void set_pdpte_base(pmap_entry_t *entry, paddr_t addr) {
  *entry |= addr & ~(PAGE_SIZE-1) & (max_phys_addr-1);
}

static inline void set_large_pde_base(pmap_entry_t *entry, paddr_t addr) {
  *entry |= addr & ~(PAE_LARGE_PAGE_SIZE-1) & (max_phys_addr-1);
}

static inline void set_huge_pdpte_base(pmap_entry_t *entry, paddr_t addr) {
  *entry |= addr & ~(PAE_HUGE_PAGE_SIZE-1) & (max_phys_addr-1);
}

static inline paddr_t get_pte_base(pmap_entry_t entry) {
  return (paddr_t)(entry & ~(PAGE_SIZE-1) & (max_phys_addr-1));
}

static inline paddr_t get_pde_base(pmap_entry_t entry) {
  return (paddr_t)(entry & ~(PAGE_SIZE-1) & (max_phys_addr-1));
}

static inline paddr_t get_pdpte_base(pmap_entry_t entry) {
  return (paddr_t)(entry & ~(PAGE_SIZE-1) & (max_phys_addr-1));
}

static inline paddr_t get_large_pde_base(pmap_entry_t entry) {
  return (paddr_t)(entry & ~(PAE_LARGE_PAGE_SIZE-1) & (max_phys_addr-1));
}

static inline paddr_t get_huge_pde_base(pmap_entry_t entry) {
  return (paddr_t)(entry & ~(PAE_HUGE_PAGE_SIZE-1) & (max_phys_addr-1));
}

extern paddr_t max_phys_addr;
extern int is_1gb_pages_supported;
extern unsigned int num_paging_levels;

#endif /* KERNEL_PAE_H */
