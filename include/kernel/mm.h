#ifndef MM_H
#define MM_H

#include <types.h>
#include <kernel/multiboot.h>
#include <kernel/lowlevel.h>
#include <kernel/paging.h>

/* FIXME: Changing any of these values may require changing
   the asm code */

#define BOOTSTRAP_VSTART    0x100000
#define BOOTSTRAP_START     0x100000

#define KERNEL_VSTART       (u32)&kVirtStart
#define PHYSMEM_START       (u32)&VPhysMemStart
#define KERNEL_START        (u32)&kPhysStart
#define RESD_PHYSMEM	    KERNEL_START

/* FIXME: Changing any of these values may require changing the 
   asm code. */

#define KERNEL_IVT		KERNEL_VSTART
#define KERNEL_IDT		0xFF400500
#define KERNEL_GDT		0xFF400D00
#define KERNEL_TSS		0xFF400F98
#define TSS_IO_PERM_BMP		0xFF401000
#define TEMP_PAGEADDR           (TSS_IO_PERM_BMP + 2 * PAGE_SIZE)
#define LAPIC_VADDR             (TEMP_PAGEADDR + PAGE_SIZE)
#define V_IDLE_STACK_TOP	(LAPIC_VADDR + 2 * PAGE_SIZE)
#define V_KERNEL_STACK_TOP	(V_IDLE_STACK_TOP + PAGE_SIZE)
#define KERNEL_VAR_ADDR		V_KERNEL_STACK_TOP
#define V_BOOTSTRAP_STACK_TOP	(KERNEL_VAR_ADDR + 2 * PAGE_SIZE)

#define INIT_PDIR		0x1000
#define FIRST_PAGE_TAB          (INIT_PDIR + PAGE_SIZE)
#define KERNEL_PAGE_TAB         FIRST_PAGE_TAB
#define INIT_SERVER_PDIR        (KERNEL_PAGE_TAB + PAGE_SIZE)
#define INIT_SERVER_USTACK_PDE  (INIT_SERVER_PDIR + PAGE_SIZE)
#define INIT_SERVER_USTACK_PTE  (INIT_SERVER_USTACK_PDE + PAGE_SIZE)
#define INIT_SERVER_PDE         (INIT_SERVER_USTACK_PTE + PAGE_SIZE)
#define IDLE_STACK_TOP		(INIT_SERVER_PDE + 2 * PAGE_SIZE)
#define KERNEL_STACK_TOP	(IDLE_STACK_TOP + PAGE_SIZE)
#define KERNEL_VAR_PAGE		KERNEL_STACK_TOP
#define BOOTSTRAP_STACK_TOP	(KERNEL_VAR_PAGE + 2 * PAGE_SIZE)

/* FIXME: end */

#define INVALID_VADDR       (void *)0xFFFFFFFF
#define INVALID_ADDR        (void *)0xFFFFFFFF

#define PAGE_ALIGN(addr)    ALIGN(PAGE_SIZE, addr)

#define ALIGN(mod, addr)    (((addr == ALIGN_DOWN(mod, addr)) ? (addr_t)addr : \
                               ((addr_t)(((u32)ALIGN_DOWN(mod, addr)) + mod)) ))

#define ALIGN_DOWN(mod, addr) ((addr_t)( (u32)addr & ~(mod - 1) ))

/* Maps a physical address to a virtual address into the current address
   space. */

#define kMapMem( virt, phys, privilege ) ( kMapPage( virt, phys, privilege) )

/* Unmaps a virtual address from the current address space. */

#define kUnmapMem( virt ) ( kUnmapPage( virt ) )

#define mapTemp( phys ) kMapMem((void *)(TEMP_PAGEADDR), phys, PAGING_RW)
#define unmapTemp() 	kUnmapMem((void *)(TEMP_PAGEADDR))

bool tempMapped;

pdir_t *kernelAddrSpace;
/*
size_t totalPhysMem;
addr_t startPhysPages;
uint32 totalPages;
uint32 totalKSize;
multiboot_info_t *grubBootInfo;
addr_t modulesStart;
size_t totalModulesSize;
*/
HOT(int _writePDE( unsigned entryNum, pde_t *pde, void * addrSpace ));
HOT(int _readPDE( unsigned entryNum, pde_t *pde, void * addrSpace ));
HOT(int readPDE( void * virt, pde_t *pde, void * addrSpace ));
HOT(int readPTE( void * virt, pte_t *pte, void * addrSpace ));
HOT(int writePTE( void * virt, pte_t *pte, void * addrSpace ));
HOT(int writePDE( void * virt, pde_t *pde, void * addrSpace ));

int mapPageTable( void *virt, void *phys, u32 privilege, void *addrSpace );
HOT(int kMapPage( void *virt, void *phys, u32 privilege ));
HOT(int mapPage( void *virt, void *phys, u32 privilege, void *addrSpace ));
HOT(addr_t kUnmapPage( void *virt ));
HOT(addr_t unmapPage( void *virt, void *addrSpace ));
addr_t unmapPageTable( void *virt, void *addrSpace );

HOT(int accessMem( void *address, size_t len, void *buffer, void *addrSpace, bool read ));
HOT(int peekMem( void *address, size_t len, void *buffer, void *addrSpace ));
HOT(int pokeMem( void *address, size_t len, void *buffer, void *addrSpace ));

//extern void addGDTEntry( word, addr_t, uint32, uint32 );

#endif
