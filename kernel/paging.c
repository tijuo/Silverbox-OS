#include <kernel/mm.h>
#include <kernel/debug.h>
#include <kernel/thread.h>
#include <kernel/memory.h>
#include <kernel/paging.h>
#include <kernel/error.h>
#include <oslib.h>
#include <string.h>
#include <kernel/bits.h>

ALIGNED(PAGE_SIZE) pte_t kMapAreaPTab[PTE_ENTRY_COUNT];

NON_NULL_PARAMS
static int accessPhys(uint64_t phys, void *buffer, size_t len, bool readPhys);

NON_NULL_PARAMS
static int accessMem(addr_t address, size_t len, void *buffer, uint32_t pdir,
bool read);

int mapLargeFrame(uint64_t phys, pmap_entry_t *pmapEntry) {
  if(phys >= MAX_PHYS_MEMORY)
    RET_MSG(EFAIL, "Error: Physical frame is out of range.");

  pde_t oldPde = readPDE(PDE_INDEX(KMAP_AREA), CURRENT_ROOT_PMAP);
  pmap_entry_t mappedPde = {
    .value = 0
  };

  setLargePdeBase(&mappedPde.largePde, PADDR_TO_PFRAME(phys));
  mappedPde.largePde.isLargePage = 1;
  mappedPde.largePde.isReadWrite = 1;
  mappedPde.largePde.isPresent = 1;

  if(IS_ERROR(
      writePDE(PDE_INDEX(KMAP_AREA), mappedPde.pde, CURRENT_ROOT_PMAP)))
    RET_MSG(E_FAIL, "Error: Unable to write PDE.");

  invalidatePage(KMAP_AREA);

  if(pmapEntry)
    pmapEntry->pde = oldPde;

  return E_OK;
}

int unmapLargeFrame(pmap_entry_t pmapEntry) {
  if(IS_ERROR(
      writePDE(PDE_INDEX(KMAP_AREA), pmapEntry.pde, CURRENT_ROOT_PMAP)))
    RET_MSG(E_FAIL, "Error: Unable to write PDE.");

  invalidatePage(KMAP_AREA);
  return E_OK;
}

/**
 Set up a new page map to be used as an address space for a thread. It mainly
 inserts the kernel page mappings.

 @param pmap The address of the page map
 @return E_OK, on success. E_FAIL, on error.
 */

int initializeRootPmap(uint32_t pmap) {
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
 * Is a particular address in an address space accessible by the user?
 *
 * @param addr The virtual address.
 * @param pdir The physical address of the page directory.
 * @param isRead Is the access a read?
 *
 * @return true if the address can be accessed. false, otherwise.
 */

bool isAccessible(addr_t addr, uint32_t pdir, bool isRead) {
  pde_t pde = readPDE(PDE_INDEX(addr), pdir);

  if(pde.isLargePage)
    return !!(pde.isPresent && (isRead || pde.isReadWrite));
  else {
    pte_t pte = readPTE(PTE_INDEX(addr), PDE_BASE(pde));
    return !!(pte.isPresent && (isRead || pte.isReadWrite));
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

pmap_entry_t readPmapEntry(uint32_t pbase, unsigned int entry) {
  assert(entry < 1024);

  if(pbase == CURRENT_ROOT_PMAP)
    pbase = getRootPageMap();

  pte_t *pte = &kMapAreaPTab[PTE_INDEX(TEMP_PAGE)];

  pte->base = ADDR_TO_PFRAME(pbase);
  pte->isReadWrite = 1;
  pte->isPresent = 1;

  invalidatePage(TEMP_PAGE);

  pmap_entry_t pmapEntry = *((pmap_entry_t*)TEMP_PAGE + entry);

  pte->isPresent = 0;

  invalidatePage(TEMP_PAGE);

  return pmapEntry;
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

int writePmapEntry(uint32_t pbase, unsigned int entry, pmap_entry_t buffer) {
  assert(entry < 1024);

  if(pbase == CURRENT_ROOT_PMAP)
    pbase = getRootPageMap();

  pte_t *pte = &kMapAreaPTab[PTE_INDEX(TEMP_PAGE)];

  pte->base = ADDR_TO_PFRAME(pbase);
  pte->isReadWrite = 1;
  pte->isPresent = 1;

  invalidatePage(TEMP_PAGE);

  pmap_entry_t *pmapEntry = ((pmap_entry_t*)TEMP_PAGE + entry);

  *pmapEntry = buffer;

  pte->isPresent = 0;

  invalidatePage(TEMP_PAGE);

  return E_OK;
}

/**
 Writes data to an address in physical memory.

 @param phys The starting physical address to which data will be copied.
 @param buffer The starting address from which data will be copied.
 @param bytes The number of bytes to be written.
 @return E_OK on success. E_INVALID_ARG if phys is NULL_PADDR.
 */

NON_NULL_PARAMS int poke(uint64_t phys, void *buffer, size_t bytes) {
  return accessPhys(phys, buffer, bytes, false);
}

/**
 Reads data from an address in physical memory.

 @param phys The starting physical address from which data will be copied.
 @param buffer The starting address to which data will be copied.
 @param bytes The number of bytes to be read.
 @return E_OK on success. E_INVALID_ARG if phys is NULL_PADDR.
 */

NON_NULL_PARAMS int peek(uint64_t phys, void *buffer, size_t bytes) {
  return accessPhys(phys, buffer, bytes, true);
}

/**
 Reads/writes data to/from an address in physical memory.

 This assumes that the memory regions don't overlap.

 @param phys The starting physical address
 @param buffer The starting address of the buffer
 @param bytes The number of bytes to be read/written.
 @param readPhys True if operation is a read, false if it's a write
 @return E_OK on success. E_BOUNDS if memory region is out of range.
 E_BOUNDS if memory access is out of range.
 */

NON_NULL_PARAMS static int accessPhys(uint64_t phys, void *buffer, size_t len,
bool readPhys)
{
  if(phys >= MAX_PHYS_MEMORY || phys + len >= MAX_PHYS_MEMORY)
    RET_MSG(E_FAIL, "Physical address is out of range.");

  pmap_entry_t oldPmapEntry;
  size_t bufferOffset = 0;

  while(len) {
    size_t physOffset = LARGE_PAGE_OFFSET(phys);
    size_t bytes =
        (len > LARGE_PAGE_SIZE - physOffset) ? LARGE_PAGE_SIZE - physOffset :
                                               len;

    if(IS_ERROR(mapLargeFrame(phys, &oldPmapEntry)))
      RET_MSG(E_FAIL, "Unable to map large page frame.");

    if(readPhys)
      memcpy((void*)((addr_t)buffer + bufferOffset),
             (void*)(KMAP_AREA + physOffset), bytes);
    else
      memcpy((void*)(KMAP_AREA + physOffset),
             (void*)((addr_t)buffer + bufferOffset), bytes);

    phys += bytes;
    bufferOffset += bytes;
    len -= bytes;
  }

  if(IS_ERROR(unmapLargeFrame(oldPmapEntry)))
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

NON_NULL_PARAMS static int accessMem(addr_t address, size_t len, void *buffer,
                                     uint32_t pdir,
                                     bool read)
{
  size_t bufferOffset = 0;

  assert(address);

  while(len) {
    uint64_t baseAddress;
    pde_t pde = readPDE(address, pdir);
    size_t addrOffset = address & (PAGE_SIZE - 1);
    size_t bytes =
        (len > PAGE_SIZE - addrOffset) ? PAGE_SIZE - addrOffset : len;

    if(pde.isPresent) {
      pmap_entry_t pmapEntry = {
        .pde = pde
      };

      if(pde.isLargePage)
        baseAddress = PFRAME_TO_PADDR(getPdeFrameNumber(pmapEntry.pde));
      else {
        pte_t pte = readPTE(address, pdir);

        if(pte.isPresent)
          baseAddress = (uint64_t)PTE_BASE(pte);
        else
          RET_MSG(E_NOT_MAPPED, "Address is not mapped");
      }
    }
    else
      RET_MSG(E_NOT_MAPPED, "Address region is not mapped");

    if(read)
      peek(baseAddress + addrOffset, (void*)((addr_t)buffer + bufferOffset),
           bytes);
    else
      poke(baseAddress + addrOffset, (void*)((addr_t)buffer + bufferOffset),
           bytes);

    address += bytes;
    bufferOffset += bytes;
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

NON_NULL_PARAMS int pokeVirt(addr_t address, size_t len, void *buffer,
                             uint32_t pdir)
{
  if(pdir == getRootPageMap() || pdir == CURRENT_ROOT_PMAP) {
    memcpy((void*)address, buffer, len);
    return E_OK;
  }
  else
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

NON_NULL_PARAMS int peekVirt(addr_t address, size_t len, void *buffer,
                             uint32_t pdir)
{
  if(pdir == getRootPageMap() || pdir == CURRENT_ROOT_PMAP) {
    memcpy(buffer, (void*)address, len);
    return E_OK;
  }
  else
    return accessMem(address, len, buffer, pdir, true);
}
