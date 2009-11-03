#ifndef PAGING_H
#define PAGING_H

#define PAGE_SIZE           0x1000
#define TABLE_SIZE          0x400000

#define PAGETAB     	    0xFFC00000
#define PAGEDIR     	    0xFFFFF000

#define ADDR_TO_PTE( addr )     (pte_t *)( PAGETAB + ( ((u32)(addr) & PAGEDIR) >> 10 ))
#define ADDR_TO_PDE( addr )     (pde_t *)( PAGEDIR + ( ((u32)(addr) & PAGETAB) >> 20 ))

#define VIRT_TO_PHYS( addr )    (addr_t)( (u32)(addr) + (u32)&kVirtToPhys )
#define PHYS_TO_VIRT( addr )    (addr_t)( (u32)(addr) + (u32)&kPhysToVirt )

#define TEMP_PTAB   0xFF400000
#define TEMP_PDIR   0xFFBFF000

#define ADDR_TO_TEMP_PTE( addr ) (pte_t *)( TEMP_PTAB + ( ((u32)(addr) & 0xFFFFF000) >> 10 ))
#define ADDR_TO_TEMP_PDE( addr ) (pde_t *)( TEMP_PDIR + ( ((u32)(addr) & 0xFFC00000) >> 20 ))

#define PAGING_PRES 		1
#define PAGING_SUPRV		0
#define PAGING_USER		4
#define PAGING_GLOBL		256
#define KB_PAGE       		0
#define MB_PAGE			128
#define DIRTY			64
#define ACCESSED		32
#define PAGING_RW		2
#define PAGING_RO		0
#define PAGING_PWT		8
#define PAGING_PCD		16

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

typedef struct {
  pde_t pageTables[1024];
} __PACKED__ pdir_t;

typedef struct {
  pte_t pages[1024];
} __PACKED__ ptab_t;

#endif /* PAGING_H */
