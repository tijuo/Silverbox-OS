#include <kernel/bits.h>
#include <kernel/debug.h>
#include <kernel/error.h>
#include <kernel/memory.h>
#include <kernel/mm.h>
#include <kernel/paging.h>
#include <kernel/thread.h>
#include <oslib.h>
#include <string.h>

ALIGN_AS(PAGE_SIZE)
pte_t kmap_area_ptab[PTE_ENTRY_COUNT];

NON_NULL_PARAMS
static int access_phys(paddr_t phys, void* buffer, size_t len, bool read_phys);

NON_NULL_PARAMS
static int access_mem(addr_t address, size_t len, void* buffer, paddr_t pdir,
    bool read);

/**
 Zero out a number of consecutive physical frames.

 @param virt Virtual start address of the kernel mapping area to be used.
 @param phys Physical start address of the frames to be cleared.
 @param count The number of 4 KiB pages.
 @returns The total number of pages that were actually cleared.
 */
unsigned int clear_phys_frames_with_start(addr_t virt, paddr_t phys, unsigned int count)
{
    unsigned int total_pages = map_temp(virt, phys, count);
    memset((void *)virt, 0, total_pages * PAGE_SIZE);

    if(unmap_temp(virt, total_pages) != total_pages) {
        kprintf("Unable to unmap temporarily mapped pages.");
    }

    return total_pages;
}

/**
 Zero out a number of consecutive physical frames.

 @param phys Physical start address of the frames to be cleared.
 @param count The number of 4 KiB pages.
 @returns The total number of pages that were actually cleared.
 */
unsigned int clear_phys_frames(paddr_t phys, unsigned int count)
{
    return clear_phys_frames_with_start((addr_t)KERNEL_TEMP_START, phys, count);
}

/**
 * Temporarily map a region of physical memory into the kernel temporary mapping area within
 * the current address space.
 * 
 * This assumes that the virtual memory region does not already contain mapped pages.
 * 
 * @param virt The starting virtual address of the mapping region. It must be
 * within KERNEL_TEMP_START and KERNEL_TEMP_END.
 * @param base The starting physical address.
 * @param count The total number of pages to be mapped.
 * @returns The actual number of pages that have been mapped (may be less than `count`).
 */
unsigned int map_temp(addr_t virt, paddr_t base, unsigned int count) {
    unsigned int i;
    addr_t addr = ALIGN_DOWN(virt, PAGE_SIZE);
    paddr_t paddr = ALIGN_DOWN(base, PAGE_SIZE);
    pte_t *pte = CURRENT_PTE(addr);

    if(addr < KERNEL_TEMP_START) {
        return 0;
    }

    for(i=0; i < count && addr < KERNEL_TEMP_END; i++, addr += PAGE_SIZE, paddr += PAGE_SIZE, pte++) {
        pte->value = 0;
        pte->base = PADDR_TO_PBASE(paddr);
        pte->is_read_write = 1;
        pte->is_present = 1;
        invalidate_page(addr);
    }

    return i;
}

/**
 * Unmap a region within the kernel temporary mapping area that had been previously
 * mapped by `map_temp()`.
 * 
 * This assumes that the virtual memory region already contains mapped pages.
 * 
 * @param virt The starting virtual address of the mapping region. It must be
 * within KERNEL_TEMP_START and KERNEL_TEMP_END.
 * @param count The total number of pages to be unmapped.
 * @returns The actual number of pages that have been unmapped (may be less than `count`).
 */
unsigned int unmap_temp(addr_t virt, unsigned int count) {
    unsigned int i;
    addr_t addr = ALIGN_DOWN(virt, PAGE_SIZE);
    pte_t *pte = CURRENT_PTE(addr);

    if(addr < KERNEL_TEMP_START) {
        return 0;
    }

    for(i=0; i < count && addr < KERNEL_TEMP_END; i++, addr += PAGE_SIZE, pte++) {
        pte->value = 0;
    }

    return i;
}

/**
 Set up a new page map to be used as an address space for a thread. It mainly
 inserts the kernel page mappings.

 Maps memory
 @param pmap The address of the page map
 @return `E_OK` - on success. `E_FAIL`, otherwise.
 */
int initialize_root_pmap(paddr_t pmap)
{
    addr_t vaddr = NDX_TO_VADDR(KERNEL_RECURSIVE_PDE, KERNEL_RECURSIVE_PDE, PDE_INDEX(KERNEL_VSTART));
    addr_t vaddr_end = NDX_TO_VADDR(KERNEL_RECURSIVE_PDE, KERNEL_RECURSIVE_PDE, PDE_INDEX(KERNEL_TEMP_END));

    pde_t pde = {
        .base = PADDR_TO_PBASE(pmap),
        .is_read_write = 1,
        .is_present = 1
    };

    // Copy the kernel PDEs

    if(IS_ERROR(poke(pmap + sizeof(pde_t) * PDE_INDEX(KERNEL_VSTART), (void *)vaddr, vaddr_end - vaddr))) {
        RET_MSG(E_FAIL, "Unable to copy kernel PDEs.");
    }

    // Recursively map the page directory into itself

    if(IS_ERROR(poke(pmap + sizeof(pde_t) * KERNEL_RECURSIVE_PDE, &pde, sizeof pde))) {
        RET_MSG(E_FAIL, "Unable to copy kernel PDEs.");
    }

    return E_OK;
}

/**
 * Is a particular address in an address space accessible by the user?
 *
 * Temporarily maps memory.
 * 
 * @param addr The virtual address.
 * @param pdir The physical address of the page directory.
 * @param is_read Is the access a read?
 *
 * @return 1 if the address can be accessed. 0, if it can't be accessed. `E_FAIL`, on failure
 */
int is_accessible(addr_t addr, paddr_t pdir, bool is_read)
{
    int ret_val;

    pde_t pde;

    if(IS_ERROR(read_pde(&pde, PDE_INDEX(addr), pdir))) {
        RET_MSG(E_FAIL, "Unable to read PDE");
    }

    if(pde.is_page_sized) {
        ret_val = (pde.is_present && (is_read || pde.is_read_write)) ? 1 : 0;
    }
    else {
        pte_t pte;

        if(IS_ERROR(read_pte(&pte, PTE_INDEX(addr), PBASE_TO_PADDR(get_pde_frame_number(pde))))) {
            RET_MSG(E_FAIL, "Unable to read PTE.");
        }

        ret_val = (pte.is_present && (is_read || pte.is_read_write)) ? 1 : 0;
    }

    return ret_val;
}

/**
 Read an arbitrary entry from a generic page map in some address space.
 (A page directory is considered to be a level 2 page map, a page table
 is a level 1 page map.)

 @param pbase The base physical address of the page map. CURRENT_ROOT_PMAP,
 if the current page map is to be used.
 @param entry The page map entry to be read.
 @param buffer The memory buffer to which the entry data will be written.
 @return E_OK on success. E_FAIL on failure.
 */
int read_pmap_entry(paddr_t pbase, unsigned int entry, pmap_entry_t *buffer)
{
    KASSERT(entry < 1024);

    if(pbase == CURRENT_ROOT_PMAP)
        pbase = get_root_page_map();

    if(map_temp((addr_t)KERNEL_TEMP_START, pbase, 1) < 1) {
        RET_MSG(E_FAIL, "Unable to map temporary page.");
    }

    *buffer = *((pmap_entry_t *)KERNEL_TEMP_START + entry);

    if(unmap_temp((addr_t)KERNEL_TEMP_START, 1) != 1) {
        RET_MSG(E_FAIL, "Unable to unmap temporary page.");
    }

    return E_OK;
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
int write_pmap_entry(paddr_t pbase, unsigned int entry, pmap_entry_t buffer)
{
    KASSERT(entry < 1024);

    if(pbase == CURRENT_ROOT_PMAP) {
        pbase = get_root_page_map();
    }

    if(map_temp((addr_t)KERNEL_TEMP_START, pbase, 1) < 1) {
        RET_MSG(E_FAIL, "Unable to map temporary page.");
    }

    pmap_entry_t *pmap_entry = (pmap_entry_t *)KERNEL_TEMP_START + entry;
    *pmap_entry = buffer;

    if(unmap_temp((addr_t)KERNEL_TEMP_START, 1) != 1) {
        RET_MSG(E_FAIL, "Unable to unmap temporary page.");
    }

    return E_OK;
}

/**
 Writes data to an address in physical memory.

 @param phys The starting physical address to which data will be copied.
 @param buffer The starting address from which data will be copied.
 @param bytes The number of bytes to be written.
 @return E_OK on success. E_INVALID_ARG if phys is NULL_PADDR.
 */
NON_NULL_PARAMS int poke(paddr_t phys, void* buffer, size_t bytes)
{
    return access_phys(phys, buffer, bytes, false);
}

/**
 Reads data from an address in physical memory.

 @param phys The starting physical address from which data will be copied.
 @param buffer The starting address to which data will be copied.
 @param bytes The number of bytes to be read.
 @return E_OK on success. E_INVALID_ARG if phys is NULL_PADDR.
 */
NON_NULL_PARAMS int peek(paddr_t phys, void* buffer, size_t bytes)
{
    return access_phys(phys, buffer, bytes, true);
}

/**
 Reads/writes data to/from an address in physical memory.

 Safety: Maps temporary pages. The buffer region should not overlap the kernel temporary mapping region.

 @param phys The starting physical address
 @param buffer The starting address of the buffer.
 @param bytes The number of bytes to be read/written.
 @param read_phys True if operation is a read, false if it's a write
 @return E_OK on success. E_BOUNDS if memory region is out of range.
 E_BOUNDS if memory access is out of range.
 */
NON_NULL_PARAMS static int access_phys(paddr_t phys, void* buffer, size_t len,
    bool read_phys)
{
    #ifdef PAE
    if(phys >= MAX_PHYS_MEMORY || phys + len >= MAX_PHYS_MEMORY)
        RET_MSG(E_FAIL, "Physical address is out of range.");
    #endif /* PAE */

    if(len == 0)
        RET_MSG(E_FAIL, "Length must be non-zero.");

    size_t buffer_offset = 0;

    while(len) {
        if(map_temp((addr_t)KERNEL_TEMP_START, phys, 1) < 1) {
            RET_MSG(E_FAIL, "Unable to map temporary page.");
        }

        size_t phys_offset = PAGE_OFFSET(phys);
        size_t bytes = (len > PAGE_SIZE - phys_offset) ? PAGE_SIZE - phys_offset : len;

        if(read_phys)
            memcpy((void*)((addr_t)buffer + buffer_offset),
                (void*)(KERNEL_TEMP_START + phys_offset), bytes);
        else
            memcpy((void*)(KERNEL_TEMP_START + phys_offset),
                (void*)((addr_t)buffer + buffer_offset), bytes);

        phys += bytes;
        buffer_offset += bytes;
        len -= bytes;

        if(unmap_temp((addr_t)KERNEL_TEMP_START, 1) < 1) {
            RET_MSG(E_FAIL, "Unable to unmap temporary page.");
        }
    }

    return E_OK;
}

/**
 Allows for reading/writing a block of memory according to the address space

 This function is used to read/write data to/from any address space. If reading,
 read len bytes from address into buffer. If writing, write len bytes from
 buffer to address.

 Safety: Maps temporary pages. The buffer region should not overlap the kernel temporary mapping region.

 @param address The address in the address space to perform the read/write.
 @param len The length of the block to read/write.
 @param buffer The buffer in the current address space that is used for the read/write.
 @param pdir The physical address of the address space.
 @param read True if reading. False if writing.
 @return E_OK on success. E_FAIL on failure.
 */
NON_NULL_PARAMS static int access_mem(addr_t address, size_t len, void* buffer,
    paddr_t pdir,
    bool read)
{
    size_t buffer_offset = 0;

    KASSERT(address);

    while(len) {
        paddr_t base_address;
        pde_t pde;

        if(IS_ERROR(read_pde(&pde, PDE_INDEX(address), pdir))) {
            RET_MSG(E_FAIL, "Unable to read PDE.");
        }

        size_t addr_offset = address & (PAGE_SIZE - 1);
        size_t bytes = (len > PAGE_SIZE - addr_offset) ? PAGE_SIZE - addr_offset : len;

        if(pde.is_present) {
            if(pde.is_page_sized) {
                base_address = PBASE_TO_PADDR(get_pde_frame_number(pde));
            }
            else {
                pte_t pte;

                if(IS_ERROR(read_pte(&pte, PTE_INDEX(address), PBASE_TO_PADDR(pde.base)))) {
                    RET_MSG(E_FAIL, "Unable to read PTE.");
                }

                if(pte.is_present) {
                    base_address = (paddr_t)PTE_BASE(pte);
                } else {
                    RET_MSG(E_NOT_MAPPED, "Address is not mapped");
                }
            }
        } else {
            RET_MSG(E_NOT_MAPPED, "Address region is not mapped");
        }

        if(read) {
            if(IS_ERROR(peek(base_address + addr_offset, (void*)((addr_t)buffer + buffer_offset), bytes))) {
                RET_MSG(E_FAIL, "Unable to read from physical memory.");
            }
        }
        else {
            if(IS_ERROR(poke(base_address + addr_offset, (void*)((addr_t)buffer + buffer_offset), bytes))) {
                RET_MSG(E_FAIL, "Unable to write to physical memory.");
            }
        }

        address += bytes;
        buffer_offset += bytes;
        len -= bytes;
    }

    return E_OK;
}

/**
 Write data to an address in an address space.

 Equivalent to an access_mem( address, len, buffer, pdir, false ).

 Safety: Maps temporary pages. The buffer region should not overlap the kernel temporary mapping region.

 @param address The address in the address space to perform the write.
 @param len The length of the block to write.
 @param buffer The buffer in the current address space that is used for the write.
 @param pdir The physical address of the address space.
 @return E_OK on success. E_FAIL on failure.
 */
NON_NULL_PARAMS int poke_virt(addr_t address, size_t len, void* buffer,
    paddr_t pdir)
{
    if(pdir == get_root_page_map() || pdir == CURRENT_ROOT_PMAP) {
        memcpy((void*)address, buffer, len);
        return E_OK;
    } else
        return access_mem(address, len, buffer, pdir, false);
}

/* data in 'address' goes to 'buffer' */

/**
 Read data from an address in an address space.

 Equivalent to an access_mem( address, len, buffer, pdir, true ).

 Safety: Maps temporary pages. The buffer region should not overlap the kernel temporary mapping region.

 @param address The address in the address space to perform the read.
 @param len The length of the block to read.
 @param buffer The buffer to store the data read from address.
 @param pdir The physical address of the root page map.
 @return E_OK on success. E_FAIL on failure.
 */
NON_NULL_PARAMS int peek_virt(addr_t address, size_t len, void* buffer,
    paddr_t pdir)
{
    if(pdir == get_root_page_map() || pdir == CURRENT_ROOT_PMAP) {
        memcpy(buffer, (void*)address, len);
        return E_OK;
    } else
        return access_mem(address, len, buffer, pdir, true);
}
