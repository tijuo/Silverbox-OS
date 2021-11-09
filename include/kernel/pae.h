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

#define PAE_PRESENT         (1ull << 0)
#define PAE_RW              (1ull << 1)
#define PAE_US              (1ull << 2)
#define PAE_PWT             (1ull << 3)
#define PAE_PCD             (1ull << 4)
#define PAE_ACCESSED        (1ull << 5)
#define PAE_DIRTY           (1ull << 6)
#define PAE_4K_PAT          (1ull << 7)
#define PAE_PS              (1ull << 7)
#define PAE_GLOBAL          (1ull << 8)
#define PAE_2MB_PAT         (1ull << 12)
#define PAE_XD              (1ull << 63)

#define LARGE_PAGE_SIZE     0x200000
#define MAX_PHYS_ADDR_BITS  52

#define PDPTE_INDEX(addr)   (size_t)(((addr) >> 30) & 0x03)
#define PDE_INDEX(addr)     (size_t)(((addr) >> 21) & 0x1FF)
#define PTE_INDEX(addr)     (size_t)(((addr) >> 12)  & 0x1FF)
#define PAGE_OFFSET(addr)   (size_t)(((addr) & 0x1FF)

#define INDEX_TO_ADDR(pdpte, pde, pte, offset)  (addr_t)((((pdpte) & 0x1FF) << 30) | (((pde) & 0x1FF) << 21) | (((pte) & 0x1FF) << 12) | (offset & 0xFFF))

typedef struct
{
  uint64_t present      : 1;
  uint64_t rwPriv       : 1;
  uint64_t usPriv       : 1;
  uint64_t pwt          : 1;
  uint64_t pcd          : 1;
  uint64_t accessed     : 1;
  uint64_t dirty        : 1;
  uint64_t pat          : 1;
  uint64_t global       : 1;
  uint64_t available    : 3;
  uint64_t base         : 40;
  uint64_t _resd        : 11;
  uint64_t noExec       : 1;
} pte_t;

typedef struct
{
  uint64_t present      : 1;
  uint64_t rwPriv       : 1;
  uint64_t usPriv       : 1;
  uint64_t pwt          : 1;
  uint64_t pcd          : 1;
  uint64_t accessed     : 1;
  uint64_t available    : 1;
  uint64_t pageSize     : 1;
  uint64_t available2   : 4;
  uint64_t base         : 40;
  uint64_t _resd        : 11;
  uint64_t noExec       : 1;
} pde_t;

typedef struct
{
  uint64_t present      : 1;
  uint64_t rwPriv       : 1;
  uint64_t usPriv       : 1;
  uint64_t pwt          : 1;
  uint64_t pcd          : 1;
  uint64_t accessed     : 1;
  uint64_t available    : 1;
  uint64_t pageSize     : 1;
  uint64_t global       : 1;
  uint64_t available2   : 3;
  uint64_t pat          : 1;
  uint64_t _resd        : 8;
  uint64_t base         : 31;
  uint64_t _resd2       : 11;
  uint64_t noExec       : 1;
} large_pde_t;

typedef struct
{
  uint64_t present      : 1;
  uint64_t _resd        : 2;
  uint64_t pwt          : 1;
  uint64_t pcd          : 1;
  uint64_t _resd2       : 4;
  uint64_t available2   : 3;
  uint64_t base         : 40;
  uint64_t _resd        : 12;
} pdpte_t;

typedef struct {
  uint32_t _resd : 5;
  uint32_t base  : 27;
} cr3_t;

#endif /* KERNEL_PAE_H */
