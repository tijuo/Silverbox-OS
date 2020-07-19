#ifndef MM_H
#define MM_H

#include <types.h>
#include <kernel/multiboot.h>
#include <kernel/lowlevel.h>
#include <kernel/paging.h>

/* FIXME: Changing any of these values may require changing
   the asm code */

#define BOOTSTRAP_VSTART    	0x1000000
#define BOOTSTRAP_START     	0x1000000

#define KERNEL_VSTART       	((addr_t)&kVirtStart)
#define PHYSMEM_START       	((addr_t)&VPhysMemStart)
#define KERNEL_START        	((addr_t)&kPhysStart)
#define RESD_PHYSMEM	    	KERNEL_START

#define PAGE_STACK            ((addr_t)&kVirtPageStack)

#define KVIRT_TO_PHYS(x)	((x) - (KERNEL_VSTART-KERNEL_START))

#define KERNEL_HEAP_START     ((addr_t)0xD0000000)
#define MIN_AMT               (8*PAGE_SIZE)
#define KERNEL_HEAP_LIMIT     ((addr_t)0xE0000000)

/* FIXME: Changing any of these values may require changing the
   asm code. */

#define TEMP_PAGEADDR           (PHYSMEM_START + 0xC0000)
#define LAPIC_VADDR             (TEMP_PAGEADDR + PAGE_SIZE)
#define KERNEL_VARIABLES	(LAPIC_VADDR + PAGE_SIZE)

#define KERNEL_CLOCK		KERNEL_VARIABLES

#define INVALID_VADDR       	((addr_t)0xFFFFFFFF)
#define INVALID_ADDR        	((addr_t)0xFFFFFFFF)

/// Aligns an address to the next page boundary (if not already aligned)
#define PAGE_ALIGN(addr)    ALIGN(PAGE_SIZE, addr)

/// Aligns an address to the next boundary (if not already aligned)
#define ALIGN(mod, addr)    (((addr == ALIGN_DOWN(mod, addr)) ? (addr_t)addr : \
                               ((addr_t)((ALIGN_DOWN(mod, addr)) + mod)) ))

/// Aligns an address to the previous boundary (if not already aligned)
#define ALIGN_DOWN(mod, addr) ((addr_t)( addr & ~(mod - 1) ))

/// Temporarily maps a physical page into the current address space
#define mapTemp( phys ) kMapPage((addr_t)(TEMP_PAGEADDR), phys, PAGING_RW)

/// Unmaps the temporary page
#define unmapTemp() 	kUnmapPage((addr_t)(TEMP_PAGEADDR), NULL)

HOT(int readPmapEntry(paddr_t pbase, int entry, void *buffer));
HOT(int writePmapEntry(paddr_t pbase, int entry, void *buffer));

HOT(int kMapPage( addr_t virt, paddr_t phys, u32 flags ));
HOT(int mapPage( addr_t virt, paddr_t phys, u32 flags, paddr_t paddrSpace ));
HOT(int kUnmapPage( addr_t virt, paddr_t *phys ));
HOT(int kMapPageTable( addr_t virt, paddr_t phys, u32 flags ));
HOT(int kUnmapPageTable( addr_t virt, paddr_t *phys ));
HOT(addr_t unmapPage( addr_t virt, paddr_t addrSpace ));

int peek( paddr_t, void *, size_t );
int poke( paddr_t, void *, size_t );

HOT(int peekVirt( addr_t address, size_t len, void *buffer, paddr_t paddrSpace ));
HOT(int pokeVirt( addr_t address, size_t len, void *buffer, paddr_t paddrSpace ));

bool is_readable( addr_t addr, paddr_t addrSpace );
bool is_writable( addr_t addr, paddr_t addrSpace );

void invalidate_tlb(void);
void invalidate_page( addr_t virt );

extern paddr_t *freePageStack, *freePageStackTop;

//extern void addGDTEntry( word, addr_t, uint32, uint32 );

paddr_t allocPageFrame(void);
void freePageFrame(paddr_t frame);

void *morecore(int amt);

#endif
