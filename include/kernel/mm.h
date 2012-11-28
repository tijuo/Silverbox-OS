#ifndef MM_H
#define MM_H

#include <types.h>
#include <kernel/multiboot.h>
#include <kernel/lowlevel.h>
#include <kernel/paging.h>

/* FIXME: Changing any of these values may require changing
   the asm code */

#define BOOTSTRAP_VSTART    	0x1000000u
#define BOOTSTRAP_START     	0x1000000u

#define KERNEL_VSTART       	((addr_t)&kVirtStart)
#define PHYSMEM_START       	((addr_t)&VPhysMemStart)
#define KERNEL_START        	((addr_t)&kPhysStart)
#define RESD_PHYSMEM	    	KERNEL_START

#define KVIRT_TO_PHYS(x)	(((u32)x) - (KERNEL_VSTART-KERNEL_START))

/* FIXME: Changing any of these values may require changing the
   asm code. */

#define TEMP_PAGEADDR           (PHYSMEM_START + 0xC0000u)
#define LAPIC_VADDR             (TEMP_PAGEADDR + PAGE_SIZE)
#define KERNEL_VARIABLES	(LAPIC_VADDR + PAGE_SIZE)

#define KERNEL_CLOCK		KERNEL_VARIABLES

#define INVALID_VADDR       	0xFFFFFFFFu
#define INVALID_ADDR        	0xFFFFFFFFu

/// Aligns an address to the next page boundary (if not already aligned)
#define PAGE_ALIGN(addr)    ALIGN(PAGE_SIZE, addr)

/// Aligns an address to the next boundary (if not already aligned)
#define ALIGN(mod, addr)    (((addr == ALIGN_DOWN(mod, addr)) ? (addr_t)addr : \
                               ((addr_t)(((u32)ALIGN_DOWN(mod, addr)) + mod)) ))

/// Aligns an address to the previous boundary (if not already aligned)
#define ALIGN_DOWN(mod, addr) ((addr_t)( (u32)addr & ~(mod - 1) ))

/// Temporarily maps a physical page into the current address space
#define mapTemp( phys ) kMapPage((addr_t)(TEMP_PAGEADDR), phys, PAGING_RW)

/// Unmaps the temporary page
#define unmapTemp() 	kUnmapPage((addr_t)(TEMP_PAGEADDR))

bool tempMapped;

pdir_t *kernelAddrSpace;

HOT(int _writePDE( unsigned int entryNum, pde_t *pde, addr_t addrSpace ));
HOT(int _readPDE( unsigned int entryNum, pde_t *pde, addr_t addrSpace ));
HOT(int readPDE( addr_t virt, pde_t *pde, addr_t addrSpace ));
HOT(int readPTE( addr_t virt, pte_t *pte, addr_t addrSpace ));
HOT(int writePTE( addr_t virt, pte_t *pte, addr_t addrSpace ));
HOT(int writePDE( addr_t virt, pde_t *pde, addr_t addrSpace ));

int mapPageTable( addr_t virt, addr_t phys, u32 flags, addr_t addrSpace );
HOT(int kMapPage( addr_t virt, addr_t phys, u32 flags ));
HOT(int mapPage( addr_t virt, addr_t phys, u32 flags, addr_t addrSpace ));
HOT(addr_t kUnmapPage( addr_t virt ));
HOT(addr_t unmapPage( addr_t virt, addr_t addrSpace ));
addr_t unmapPageTable( addr_t virt, addr_t addrSpace );

int peek( addr_t, addr_t, size_t );
int poke( addr_t, addr_t, size_t );

HOT(int peekVirt( addr_t address, size_t len, addr_t buffer, addr_t addrSpace ));
HOT(int pokeVirt( addr_t address, size_t len, addr_t buffer, addr_t addrSpace ));

bool is_readable( addr_t addr, addr_t addrSpace );
bool is_writable( addr_t addr, addr_t addrSpace );

//extern void addGDTEntry( word, addr_t, uint32, uint32 );

#endif
