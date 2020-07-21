#ifndef PAGING_H
#define PAGING_H

#define PAGE_SIZE		      0x1000
#define LARGE_PAGE_SIZE   0x400000
#define PAGE_TABLE_SIZE	  0x400000

#define PAGETAB			0xFFC00000
#define PAGEDIR			0xFFFFF000

#define ADDR_TO_PTE( addr )	(pte_t *)( PAGETAB + ( ((addr) & 0xFFFFF000) >> 10 ))
#define ADDR_TO_PDE( addr )	(pde_t *)( PAGEDIR + ( ((addr) & 0xFFC00000) >> 20 ))

#define VIRT_TO_PHYS( addr )	(addr_t)( (addr) + &kVirtToPhys )
#define PHYS_TO_VIRT( addr )	(addr_t)( (addr) + &kPhysToVirt )

#define PDE_INDEX(a)		(((a) >> 22) & 0x3FF)
#define PTE_INDEX(a)		(((a) >> 12) & 0x3FF)
#define PAGE_OFFSET(a)		((a) & 0xFFF)

#define IDX_TO_VADDR(pde, pte, offset)  (((pde & 0x3FF) << 22) | ((pte & 0x3FF) << 12) | (offset & 0xFFF))

#define PAGING_PRES		(1u << 0)
#define PAGING_RW		(1u << 1)
#define PAGING_RO		0u
#define PAGING_SUPERVISOR	0u
#define PAGING_USER		(1u << 2)
#define PAGING_PWT		(1u << 3)
#define PAGING_PCD		(1u << 4)
#define PAGING_ACCESSED		(1u << 5)
#define PAGING_DIRTY		(1u << 6)
#define PAGING_4KB_PAGE		0u
#define PAGING_4MB_PAGE		(1u << 7)
#define PAGING_GLOBAL		(1u << 8)

#define PAGING_ERR_PRES		(1u << 0)
#define PAGING_ERR_READ		0u
#define PAGING_ERR_WRITE	(1u << 1)
#define PAGING_ERR_SUPERVISOR	0u
#define PAGING_ERR_USER		(1u << 2)

struct CR3_Struct
{
  dword _resd1 : 3;
  dword pwt    : 1;
  dword pcd    : 1;
  dword _resd2 : 7;
  dword base   : 20;
};

typedef struct CR3_Struct cr3_t;

struct PageMapEntry
{
  dword present   : 1;
  dword rwPriv    : 1;
  dword usPriv    : 1;
  dword pwt       : 1;
  dword pcd       : 1;
  dword accessed  : 1;
  dword dirty     : 1;
  dword pageSize  : 1;
  dword global    : 1;
  dword available : 3;
  dword base      : 20;
};

typedef struct PageMapEntry pmap_t;
typedef pmap_t pte_t;
typedef pmap_t pde_t;

/*
/// Represents an x86 PTE in a page table

struct PageTableEntry
{
  dword present  : 1;
  dword rwPriv   : 1;
  dword usPriv   : 1;
  dword pwt      : 1;
  dword pcd      : 1;
  dword accessed : 1;
  dword dirty    : 1;
  dword reserved : 1;
  dword global   : 1;
  dword avail    : 3;
  dword base     : 20;
} __PACKED__;

typedef struct PageTableEntry pte_t;

/// Represents an x86 PDE in a page directory

struct PageDirEntry
{
  dword present  : 1;
  dword rwPriv   : 1;
  dword usPriv   : 1;
  dword pwt      : 1;
  dword pcd      : 1;
  dword accessed : 1;
  dword reserved : 1;
  dword pSize    : 1;
  dword global   : 1;
  dword avail    : 3;
  dword base     : 20;
} __PACKED__;

typedef struct PageDirEntry pde_t;
*/

/// Represents an x86 page directory

typedef struct {
  pde_t entries[1024];
} __PACKED__ pdir_t;

/// Represents an x86 page table

typedef struct {
  pte_t entries[1024];
} __PACKED__ ptab_t;

#endif /* PAGING_H */
