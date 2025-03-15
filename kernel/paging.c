#include <kernel/bits.h>
#include <kernel/debug.h>
#include <kernel/error.h>
#include <kernel/memory.h>
#include <kernel/mm.h>
#include <kernel/paging.h>
#include <kernel/thread.h>
#include <oslib.h>
#include <string.h>

ALIGNED(PAGE_SIZE)
pte_t kmap_area_ptab[PTE_ENTRY_COUNT];

NON_NULL_PARAMS
static int access_phys(uint64_t phys, void* buffer, size_t len, bool read_phys);

NON_NULL_PARAMS
static int access_mem(addr_t address, size_t len, void* buffer, uint32_t pdir,
    bool read);

/**
 Zero out a page frame.

 @param phys Physical address frame to clear.
 @return E_OK on success. E_FAIL on failure.
 */

int clear_phys_page(uint32_t phys)
{
    pmap_entry_t old_pmap_entry;
    size_t offset = LARGE_PAGE_OFFSET(ALIGN_DOWN(phys, (uint64_t)PAGE_SIZE));

    if(IS_ERROR(map_large_frame(phys, &old_pmap_entry)))
        RET_MSG(E_FAIL, "Unable to map physical frame.");

    uint32_t* ptr = (uint32_t*)(KMAP_AREA + offset);

    for(size_t i = 0; i < 1024; i++)
        ptr[i] = 0;

    if(IS_ERROR(unmap_large_frame(old_pmap_entry)))
        RET_MSG(E_FAIL, "Unable to unmap physical frame.");

    return E_OK;
}

int map_large_frame(uint64_t phys, pmap_entry_t* pmap_entry)
{
    if(phys >= MAX_PHYS_MEMORY)
        RET_MSG(EFAIL, "Error: Physical frame is out of range.");

    pde_t old_pde = read_pde(PDE_INDEX(KMAP_AREA), CURRENT_ROOT_PMAP);
    pmap_entry_t mapped_pde = {
        .value = 0
    };

    set_large_pde_base(&mapped_pde.large_pde, PADDR_TO_PFRAME(phys));
    mapped_pde.large_pde.is_large_page = 1;
    mapped_pde.large_pde.is_read_write = 1;
    mapped_pde.large_pde.is_present = 1;

    if(IS_ERROR(
        write_pde(PDE_INDEX(KMAP_AREA), mapped_pde.pde, CURRENT_ROOT_PMAP)))
        RET_MSG(E_FAIL, "Error: Unable to write PDE.");

    invalidate_page(KMAP_AREA);

    if(pmap_entry)
        pmap_entry->pde = old_pde;

    return E_OK;
}

int unmap_large_frame(pmap_entry_t pmap_entry)
{
    if(IS_ERROR(
        write_pde(PDE_INDEX(KMAP_AREA), pmap_entry.pde, CURRENT_ROOT_PMAP)))
        RET_MSG(E_FAIL, "Error: Unable to write PDE.");

    invalidate_page(KMAP_AREA);
    return E_OK;
}

/**
 Set up a new page map to be used as an address space for a thread. It mainly
 inserts the kernel page mappings.

 @param pmap The address of the page map
 @return E_OK, on success. E_FAIL, on error.
 */

int initialize_root_pmap(uint32_t pmap)
{
    // Map kernel memory into the new address space

#ifdef DEBUG

    // map the first page table

    if(IS_ERROR(write_pde(0, read_pde(0, CURRENT_ROOT_PMAP), pmap)))
        RET_MSG(E_FAIL, "Unable to copy kernel PDEs (write failed).");

#endif /* DEBUG */

    for(unsigned int entry = PDE_INDEX(KERNEL_VSTART); entry < 1024; entry++) {
        if(IS_ERROR(write_pde(entry, read_pde(entry, CURRENT_ROOT_PMAP), pmap)))
            RET_MSG(E_FAIL, "Unable to copy kernel PDEs (write failed).");
    }

    return E_OK;
}

/**
 * Is a particular address in an address space accessible by the user?
 *
 * @param addr The virtual address.
 * @param pdir The physical address of the page directory.
 * @param is_read Is the access a read?
 *
 * @return true if the address can be accessed. false, otherwise.
 */

bool is_accessible(addr_t addr, uint32_t pdir, bool is_read)
{
    pde_t pde = read_pde(PDE_INDEX(addr), pdir);

    if(pde.is_large_page)
        return !!(pde.is_present && (is_read || pde.is_read_write));
    else {
        pte_t pte = read_pte(PTE_INDEX(addr), PDE_BASE(pde));
        return !!(pte.is_present && (is_read || pte.is_read_write));
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

pmap_entry_t read_pmap_entry(uint32_t pbase, unsigned int entry)
{
    kassert(entry < 1024);

    if(pbase == CURRENT_ROOT_PMAP)
        pbase = get_root_page_map();

    pte_t* pte = &kmap_area_ptab[PTE_INDEX(TEMP_PAGE)];

    pte->base = ADDR_TO_PFRAME(pbase);
    pte->is_read_write = 1;
    pte->is_present = 1;

    invalidate_page(TEMP_PAGE);

    pmap_entry_t pmap_entry = *((pmap_entry_t*)TEMP_PAGE + entry);

    pte->is_present = 0;

    invalidate_page(TEMP_PAGE);

    return pmap_entry;
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

int write_pmap_entry(uint32_t pbase, unsigned int entry, pmap_entry_t buffer)
{
    kassert(entry < 1024);

    if(pbase == CURRENT_ROOT_PMAP)
        pbase = get_root_page_map();

    pte_t* pte = &kmap_area_ptab[PTE_INDEX(TEMP_PAGE)];

    pte->base = ADDR_TO_PFRAME(pbase);
    pte->is_read_write = 1;
    pte->is_present = 1;

    invalidate_page(TEMP_PAGE);

    pmap_entry_t* pmap_entry = ((pmap_entry_t*)TEMP_PAGE + entry);

    *pmap_entry = buffer;

    pte->is_present = 0;

    invalidate_page(TEMP_PAGE);

    return E_OK;
}

/**
 Writes data to an address in physical memory.

 @param phys The starting physical address to which data will be copied.
 @param buffer The starting address from which data will be copied.
 @param bytes The number of bytes to be written.
 @return E_OK on success. E_INVALID_ARG if phys is NULL_PADDR.
 */

NON_NULL_PARAMS int poke(uint64_t phys, void* buffer, size_t bytes)
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

NON_NULL_PARAMS int peek(uint64_t phys, void* buffer, size_t bytes)
{
    return access_phys(phys, buffer, bytes, true);
}

/**
 Reads/writes data to/from an address in physical memory.

 This assumes that the memory regions don't overlap.

 @param phys The starting physical address
 @param buffer The starting address of the buffer
 @param bytes The number of bytes to be read/written.
 @param read_phys True if operation is a read, false if it's a write
 @return E_OK on success. E_BOUNDS if memory region is out of range.
 E_BOUNDS if memory access is out of range.
 */

NON_NULL_PARAMS static int access_phys(uint64_t phys, void* buffer, size_t len,
    bool read_phys)
{
    if(phys >= MAX_PHYS_MEMORY || phys + len >= MAX_PHYS_MEMORY)
        RET_MSG(E_FAIL, "Physical address is out of range.");
    else if(len == 0)
        RET_MSG(E_FAIL, "Length must be non-zero.");

    pmap_entry_t old_pmap_entry = {};
    size_t buffer_offset = 0;

    while(len) {
        size_t phys_offset = LARGE_PAGE_OFFSET(phys);
        size_t bytes = (len > LARGE_PAGE_SIZE - phys_offset) ? LARGE_PAGE_SIZE - phys_offset : len;

        if(IS_ERROR(map_large_frame(phys, &old_pmap_entry)))
            RET_MSG(E_FAIL, "Unable to map large page frame.");

        if(read_phys)
            memcpy((void*)((addr_t)buffer + buffer_offset),
                (void*)(KMAP_AREA + phys_offset), bytes);
        else
            memcpy((void*)(KMAP_AREA + phys_offset),
                (void*)((addr_t)buffer + buffer_offset), bytes);

        phys += bytes;
        buffer_offset += bytes;
        len -= bytes;
    }

    if(IS_ERROR(unmap_large_frame(old_pmap_entry)))
        RET_MSG(E_FAIL, "Unable to unmap large page frame.");

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

NON_NULL_PARAMS static int access_mem(addr_t address, size_t len, void* buffer,
    uint32_t pdir,
    bool read)
{
    size_t buffer_offset = 0;

    kassert(address);

    while(len) {
        uint64_t base_address;
        pde_t pde = read_pde(address, pdir);
        size_t addr_offset = address & (PAGE_SIZE - 1);
        size_t bytes = (len > PAGE_SIZE - addr_offset) ? PAGE_SIZE - addr_offset : len;

        if(pde.is_present) {
            pmap_entry_t pmap_entry = {
                .pde = pde
            };

            if(pde.is_large_page)
                base_address = PFRAME_TO_PADDR(get_pde_frame_number(pmap_entry.pde));
            else {
                pte_t pte = read_pte(address, pdir);

                if(pte.is_present)
                    base_address = (uint64_t)PTE_BASE(pte);
                else
                    RET_MSG(E_NOT_MAPPED, "Address is not mapped");
            }
        } else
            RET_MSG(E_NOT_MAPPED, "Address region is not mapped");

        if(read)
            peek(base_address + addr_offset, (void*)((addr_t)buffer + buffer_offset),
                bytes);
        else
            poke(base_address + addr_offset, (void*)((addr_t)buffer + buffer_offset),
                bytes);

        address += bytes;
        buffer_offset += bytes;
        len -= bytes;
    }

    return E_OK;
}

/**
 Write data to an address in an address space.

 Equivalent to an access_mem( address, len, buffer, pdir, false ).

 @param address The address in the address space to perform the write.
 @param len The length of the block to write.
 @param buffer The buffer in the current address space that is used for the write.
 @param pdir The physical address of the address space.
 @return E_OK on success. E_FAIL on failure.
 */

NON_NULL_PARAMS int poke_virt(addr_t address, size_t len, void* buffer,
    uint32_t pdir)
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

 @param address The address in the address space to perform the read.
 @param len The length of the block to read.
 @param buffer The buffer to store the data read from address.
 @param pdir The physical address of the root page map.
 @return E_OK on success. E_FAIL on failure.
 */

NON_NULL_PARAMS int peek_virt(addr_t address, size_t len, void* buffer,
    uint32_t pdir)
{
    if(pdir == get_root_page_map() || pdir == CURRENT_ROOT_PMAP) {
        memcpy(buffer, (void*)address, len);
        return E_OK;
    } else
        return access_mem(address, len, buffer, pdir, true);
}
