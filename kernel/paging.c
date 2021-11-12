#include <kernel/mm.h>
#include <kernel/debug.h>
#include <kernel/thread.h>
#include <kernel/memory.h>
#include <kernel/paging.h>
#include <kernel/error.h>
#include <oslib.h>
#include <string.h>
#include <kernel/bits.h>

NON_NULL_PARAMS
static int accessPhys(paddr_t phys, void *buffer, size_t len, bool readPhys);

NON_NULL_PARAMS
static int accessMem(addr_t address, size_t len, void *buffer, paddr_t pdir,
bool read);

/**
 Set up a new page map to be used as an address space for a thread. Inserts the
 kernel page mappings and the recursive map.

 @param pmap The address of the page map
 @return E_OK, on success. E_FAIL, on error.
 */

int initializeRootPmap(paddr_t pmap) {
	// Map kernel memory into the new address space

#ifdef DEBUG

	// map the first page table

	if(IS_ERROR(writePDE(0, readPDE(0, CURRENT_ROOT_PMAP), pmap)))
		RET_MSG(E_FAIL, "Unable to copy kernel PDEs (write failed).");

#endif /* DEBUG */

	for(unsigned int entry = PDE_INDEX(KERNEL_VSTART); entry < 1024; entry++) {
		if(IS_ERROR(writePDE(entry, readPDE(entry, CURRENT_ROOT_PMAP), pmap)))
			RET_MSG(E_FAIL, "Unable to copy kernel PDEs (write failed).");
	}

	return E_OK;
}

/**
 Zero out a page frame.

 @param phys Physical address frame to clear.
 @return E_OK on success. E_FAIL on failure.
 */

int clearPhysPage(paddr_t phys) {
	assert(phys < MAX_PHYS_MEMORY);

	clearMemory((void*)KPHYS_TO_VIRT((uintptr_t )phys), PAGE_SIZE);
	return E_OK;
}

bool isAccessible(addr_t addr, paddr_t pdir, bool isReadOnly) {
	pde_t pde = readPDE(PDE_INDEX(addr), pdir);

	if(pde.isLargePage)
		return !!(pde.isPresent && (isReadOnly || pde.isReadWrite));
	else {
		pte_t pte = readPTE(PTE_INDEX(addr), PDE_BASE(pde));
		return !!(pte.isPresent && (isReadOnly || pte.isReadWrite));
	}
}

/**
 Read an arbitrary entry from a generic page map in some address space.
 (A page directory is considered to be a level 2 page map, a page table
 is a level 1 page map.)

 @param pbase The base physical address of the page map. CURRENT_ROOT_PMAP,
 if the current page map is to be used.
 @param entry The page map entry to be read.
 @param buffer The memory buffer to which the entry data will be written.
 @return E_OK on success. E_FAIL on failure. E_NOT_MAPPED if the
 PDE corresponding to the virtual address hasn't been mapped
 yet.
 */

pmap_entry_t readPmapEntry(paddr_t pbase, unsigned int entry) {
	if(pbase == CURRENT_ROOT_PMAP)
		pbase = getRootPageMap();

	assert(entry < 1024);
	assert(pbase < MAX_PHYS_MEMORY);

	pmap_entry_t *pmap_entry = (pmap_entry_t*)KPHYS_TO_VIRT((uintptr_t )pbase);

	return pmap_entry[entry];
}

/**
 Write an arbitrary entry to a generic page map in some address space.
 (A page directory is considered to be a level 2 page map, a page table
 is a level 1 page map.)
 @param pbase The base physical address of the page map. CURRENT_ROOT_PMAP
 if the current page map is to be used.
 @param entry The page map entry to be written.
 @param buffer The memory buffer from which the entry data will be read.
 @return E_OK on success. E_FAIL on failure. E_NOT_MAPPED if the
 PDE corresponding to the virtual address hasn't been mapped
 yet.
 */

int writePmapEntry(paddr_t pbase, unsigned int entry, pmap_entry_t buffer) {
	if(pbase == CURRENT_ROOT_PMAP)
		pbase = getRootPageMap();

	assert(pbase < MAX_PHYS_MEMORY);
	assert(entry < 1024);

	pmap_entry_t *pmap_entry = (pmap_entry_t*)KPHYS_TO_VIRT((uintptr_t )pbase);

	pmap_entry[entry] = buffer;
	return E_OK;
}

/**
 Writes data to an address in physical memory.

 @param phys The starting physical address to which data will be copied.
 @param buffer The starting address from which data will be copied.
 @param bytes The number of bytes to be written.
 @return E_OK on success. E_INVALID_ARG if phys is NULL_PADDR.
 */

NON_NULL_PARAMS
int poke(paddr_t phys, void *buffer, size_t bytes) {
	return accessPhys(phys, buffer, bytes, false);
}

/**
 Reads data from an address in physical memory.

 @param phys The starting physical address from which data will be copied.
 @param buffer The starting address to which data will be copied.
 @param bytes The number of bytes to be read.
 @return E_OK on success. E_INVALID_ARG if phys is NULL_PADDR.
 */

NON_NULL_PARAMS
int peek(paddr_t phys, void *buffer, size_t bytes) {
	return accessPhys(phys, buffer, bytes, true);
}

/**
 Reads/writes data to/from an address in physical memory.

 This assumes that the memory regions don't overlap.

 @param phys The starting physical address
 @param buffer The starting address of the buffer
 @param bytes The number of bytes to be read/written.
 @param readPhys True if operation is a read, false if it's a write
 @return E_OK on success. E_INVALID_ARG if phys is NULL_PADDR.
 E_BOUNDS if memory access is out of range.
 */

NON_NULL_PARAMS
static int accessPhys(paddr_t phys, void *buffer, size_t len, bool readPhys) {
	assert(phys < MAX_PHYS_MEMORY);

	addr_t mappedPhys = KPHYS_TO_VIRT((uintptr_t )phys);

	if(phys == NULL_PADDR)
		RET_MSG(E_INVALID_ARG, "Invalid physical address.");
	else if(len > MAX_PHYS_MEMORY || phys > MAX_PHYS_MEMORY - len)
		RET_MSG(E_BOUNDS, "Attempted to access physical memory past 2 GiB limit.");

	if(readPhys)
		memcpy(buffer, (void*)mappedPhys, len);
	else
		memcpy((void*)mappedPhys, buffer, len);

	return E_OK;
}

/**
 Allows for reading/writing a block of memory according to the address space

 This function is used to read/write data to/from any address space. If reading,
 read len bytes from address into buffer. If writing, write len bytes from
 buffer to address.

 This assumes that the memory regions don't overlap.

 @param address The address in the address space to perform the read/write.
 @param len The length of the block to read/write.
 @param buffer The buffer in the current address space that is used for the read/write.
 @param pdir The physical address of the address space.
 @param read True if reading. False if writing.
 @return E_OK on success. E_FAIL on failure.
 */

NON_NULL_PARAMS
static int accessMem(addr_t address, size_t len, void *buffer, paddr_t pdir,
bool read) {
	size_t buffer_offset = 0;

	assert(address);

	while(len) {
		paddr_t baseAddress;
		pde_t pde = readPDE(address, pdir);
		size_t addr_offset = address & (PAGE_SIZE - 1);
		size_t bytes =
				(len > PAGE_SIZE - addr_offset) ? PAGE_SIZE - addr_offset : len;

		if(pde.isPresent)
		{
			pmap_entry_t pmapEntry = { .pde = pde };

			if(pde.isLargePage)
				baseAddress = LARGE_PDE_BASE(pmapEntry.largePde);
			else
			{
				pte_t pte = readPTE(address, pdir);

				if(pte.isPresent)
					baseAddress = PTE_BASE(pte);
				else
					RET_MSG(E_NOT_MAPPED, "Address is not mapped");
			}
		}
		else
			RET_MSG(E_NOT_MAPPED, "Address region is not mapped");

		if(read)
			peek(baseAddress + addr_offset,
					(void*)((addr_t)buffer + buffer_offset), bytes);
		else
			poke(baseAddress + addr_offset,
					(void*)((addr_t)buffer + buffer_offset), bytes);

		address += bytes;
		buffer_offset += bytes;
		len -= bytes;
	}

	return E_OK;
}

/**
 Write data to an address in an address space.

 Equivalent to an accessMem( address, len, buffer, pdir, false ).

 @param address The address in the address space to perform the write.
 @param len The length of the block to write.
 @param buffer The buffer in the current address space that is used for the write.
 @param pdir The physical address of the address space.
 @return E_OK on success. E_FAIL on failure.
 */

NON_NULL_PARAMS
int pokeVirt(addr_t address, size_t len, void *buffer, paddr_t pdir) {
	if(pdir == getRootPageMap() || pdir == CURRENT_ROOT_PMAP) {
		memcpy((void*)address, buffer, len);
		return E_OK;
	} else
		return accessMem(address, len, buffer, pdir, false);
}

/* data in 'address' goes to 'buffer' */

/**
 Read data from an address in an address space.

 Equivalent to an accessMem( address, len, buffer, pdir, true ).

 @param address The address in the address space to perform the read.
 @param len The length of the block to read.
 @param buffer The buffer to store the data read from address.
 @param pdir The physical address of the root page map.
 @return E_OK on success. E_FAIL on failure.
 */

NON_NULL_PARAMS
int peekVirt(addr_t address, size_t len, void *buffer, paddr_t pdir) {
	if(pdir == getRootPageMap() || pdir == CURRENT_ROOT_PMAP) {
		memcpy(buffer, (void*)address, len);
		return E_OK;
	} else
		return accessMem(address, len, buffer, pdir, true);
}

/**
 Maps a page in physical memory to virtual memory in the current address space

 @param virt The virtual address of the page.
 @param phys The physical address of the page.
 @param flags The flags that modify page properties.
 @return E_OK on success. E_INVALID_ARG if virt is invalid or phys is NULL.
 E_OVERWRITE if a mapping already exists.
 E_NOT_MAPPED if the page table of the page isn't mapped.
 E_FAIL on other failure.
 */

int kMapPage(addr_t virt, paddr_t phys, u32 flags) {
	assert(virt);
	assert(phys != NULL_PADDR);
	assert(phys < MAX_PHYS_MEMORY);

	pde_t pde = readPDE(PDE_INDEX(virt), CURRENT_ROOT_PMAP);

	if(IS_FLAG_SET(flags, PAGING_4MB_PAGE)) {
		if(pde.isPresent) {
			kprintf("kMapPage(): %#x -> %#llx\n", virt, phys);
			RET_MSG(E_OVERWRITE, "Mapping already exists for virtual address.");
		}

		pmap_entry_t pmapEntry = { .pde = pde };

		setLargePdeBase(&pmapEntry.largePde, phys);
		pmapEntry.value |= (flags & LARGE_PDE_FLAG_MASK) | PAGING_PRES;

		if(IS_ERROR(writePDE(PDE_INDEX(virt), pmapEntry.pde, CURRENT_ROOT_PMAP)))
			RET_MSG(E_FAIL, "Unable to write PDE.");

		invalidatePage(ALIGN_DOWN(virt, LARGE_PAGE_SIZE));
	} else {
		pte_t pte;

		if(!pde.isPresent) {
			kprintf("kMapPage(): %#x -> %#llx\n", virt, phys);
			RET_MSG(E_NOT_MAPPED, "PDE is not present for virtual address.");
		}

		pte = readPTE(PTE_INDEX(virt), PDE_BASE(pde));

		if(pte.isPresent) {
			kprintf("kMapPage(): %#x -> %#llx\n", virt, phys);
			RET_MSG(E_OVERWRITE, "Mapping already exists for virtual address.");
		}

		pte.value = ((uint32_t)phys & PAGE_BASE_MASK) | (flags & PTE_FLAG_MASK)
			| PAGING_PRES;

		if(IS_ERROR(writePTE(PTE_INDEX(virt), pte, PDE_BASE(pde))))
			RET_MSG(E_FAIL, "Unable to write PTE.");
		invalidatePage(virt);
	}

	return E_OK;
}

/**
 Unmaps a page from the current address space

 @param virt The virtual address of the page to unmap.
 @return E_OK on success with *phys set to the physical address of the page frame.
 E_NOT_MAPPED if address was already unmapped. E_FAIL on failure.
 */

int kUnmapPage(addr_t virt, paddr_t *phys) {
	pte_t pte;

	pmap_entry_t pmapEntry = { .pde = readPDE(PDE_INDEX(virt), CURRENT_ROOT_PMAP) };

	if(pmapEntry.pde.isPresent) {
		if(!pmapEntry.pde.isLargePage) {
			pte = readPTE(PTE_INDEX(virt), PDE_BASE(pmapEntry.pde));

			if(pte.isPresent) {
				if(phys)
					*phys = PTE_BASE(pte) | (pte.value & PTE_FLAG_MASK);

				pte.isPresent = 0;

				if(IS_ERROR(writePTE(PTE_INDEX(virt), pte, PDE_BASE(pmapEntry.pde))))
					RET_MSG(E_FAIL, "Unable to write PDE.");
			} else
				RET_MSG(E_NOT_MAPPED, "Virtual address is not mapped.");
		} else {
			if(phys)
				*phys = LARGE_PDE_BASE(pmapEntry.largePde)
					| (pmapEntry.value & LARGE_PDE_FLAG_MASK);

			pmapEntry.largePde.isPresent = 0;

			if(IS_ERROR(writePDE(PDE_INDEX(virt), pmapEntry.pde, CURRENT_ROOT_PMAP)))
				RET_MSG(E_FAIL, "Unable to write PDE.");
		}
	} else
		RET_MSG(E_NOT_MAPPED, "PDE is not present for virtual address.");

	invalidatePage(
			ALIGN_DOWN(virt, pmapEntry.pde.isLargePage ? LARGE_PAGE_SIZE : PAGE_SIZE));

	return E_OK;
}

/**
 Maps a page in physical memory to virtual memory in the current address space

 @param virt The virtual address of the page.
 @param phys The physical address of the page.
 @param flags The flags that modify page properties.
 @return E_OK on success. E_INVALID_ARG if phys is NULL_PADDR.
 E_OVERWRITE if a mapping already exists. E_FAIL on failure.
 */

int kMapPageTable(addr_t virt, paddr_t phys, u32 flags) {
	if(phys == NULL_PADDR) {
		kprintf("kMapPageTable(): %#x -> %#llx: ", virt, phys);
		RET_MSG(E_INVALID_ARG, "Invalid physical address");
	}

	pmap_entry_t pmapEntry = { .pde = readPDE(PDE_INDEX(virt), CURRENT_ROOT_PMAP) };

	if(pmapEntry.pde.isPresent) {
		kprintf("kMapPageTable(): %#x -> %#llx:\n", virt, phys);
		RET_MSG(E_OVERWRITE, "Address is already mapped!");
	}

	if(IS_FLAG_SET(flags, PAGING_4MB_PAGE)) {
		setLargePdeBase(&pmapEntry.largePde, phys);
		pmapEntry.value |= (flags & LARGE_PDE_FLAG_MASK) | PAGING_PRES;
	} else
		pmapEntry.value = (uint32_t)phys | (flags & PDE_FLAG_MASK) | PAGING_PRES;

	if(IS_ERROR(writePDE(PDE_INDEX(virt), pmapEntry.pde, CURRENT_ROOT_PMAP)))
		RET_MSG(E_FAIL, "Unable to write PDE.");

	return E_OK;
}

/**
 Unmaps a page table from the current address space

 @param virt The virtual address of the page.
 @param phys A pointer for the unmapped physical address of the page table that
 will be returned
 @return E_OK on success. E_NOT_MAPPED if mapping doesn't exist. E_FAIL on failure.
 */

int kUnmapPageTable(addr_t virt, paddr_t *phys) {
	pmap_entry_t pmapEntry = { .pde = readPDE(PDE_INDEX(virt), CURRENT_ROOT_PMAP) };

	if(!pmapEntry.pde.isPresent)
		RET_MSG(E_NOT_MAPPED, "PDE is not present for virtual address.");

	pmapEntry.pde.isPresent = 0;

	if(pmapEntry.pde.isLargePage) {
		if(phys)
			*phys = LARGE_PDE_BASE(pmapEntry.largePde)
				| (pmapEntry.value & LARGE_PDE_FLAG_MASK);

		invalidatePage(ALIGN_DOWN(virt, LARGE_PAGE_SIZE));
	} else {
		if(phys)
			*phys = PDE_BASE(pmapEntry.pde) | (pmapEntry.value & PDE_FLAG_MASK);

		addr_t tableBaseAddr = ALIGN_DOWN(virt, PAGE_TABLE_SIZE);

		for(addr_t addr = tableBaseAddr; addr < tableBaseAddr + PAGE_TABLE_SIZE;
				addr += PAGE_SIZE)
			invalidatePage(addr);
	}

	if(IS_ERROR(writePDE(PDE_INDEX(virt), pmapEntry.pde, CURRENT_ROOT_PMAP)))
		RET_MSG(E_FAIL, "Unable to write PDE.");

	return E_OK;
}
