#include <kernel/mm.h>
#include <kernel/debug.h>
#include <kernel/thread.h>
#include <kernel/memory.h>
#include <kernel/paging.h>
#include <kernel/error.h>
#include <oslib.h>
#include <string.h>
#include <kernel/bits.h>

size_t largePageSize=LARGE_PAGE_SIZE;

static int readPDE( unsigned entryNum, pde_t *pde, paddr_t pdir );
static int readPTE( addr_t virt, pte_t *pte, paddr_t pdir );

//int readPmapEntry(paddr_t pbase, int entry, void *buffer);
//int writePmapEntry(paddr_t pbase, int entry, void *buffer);

static int accessPhys( paddr_t phys, void *buffer, size_t len, bool readPhys );
static int accessMem( addr_t address, size_t len, void *buffer, paddr_t pdir,
                      bool read );
/*
int pokeVirt( addr_t address, size_t len, void *buffer, paddr_t pdir );
int peekVirt( addr_t address, size_t len, void *buffer, paddr_t pdir );

int poke( paddr_t phys, void *buffer, size_t bytes );
int peek( paddr_t phys, void *buffer, size_t bytes );

bool isReadable( addr_t addr, paddr_t pdir );
bool isWritable( addr_t addr, paddr_t pdir );
 */

/**
  Set up a new page map to be used as an address space for a thread. Inserts the
  kernel page mappings and the recursive map.

  @param pmap The address of the page map
  @return E_OK, on success. E_FAIL, on error.
 */

int initializeRootPmap(dword pmap)
{
  size_t bufSize = sizeof(pde_t)*(1023-PDE_INDEX(KERNEL_TCB_START));
  char buf[bufSize];

  // Map the page directory, kernel space, and first page table
  // into the new address space

  u32 pentry = (u32)pmap | PAGING_RW | PAGING_PRES;

  // recursively map the page map to itself

  if(IS_ERROR(writePmapEntry(pmap, PDE_INDEX(PAGETAB), &pentry)))
    RET_MSG(E_FAIL, "Unable to perform recursive mapping.")

///# if DEBUG

  // map the first page table

  if(IS_ERROR(writePmapEntry(pmap, 0, (void *)PAGEDIR)))
    RET_MSG(E_FAIL, "Unable to map first page table.")
//# endif /* DEBUG */

  // Copy any page tables in the kernel region of virtual memory from
  // the bootstrap address space to the thread's address space

  if(IS_ERROR(peek(initKrnlPDir + sizeof(pde_t) * PDE_INDEX(KERNEL_TCB_START), (void *)buf, bufSize)))
    RET_MSG(E_FAIL, "Unable to peek kernel PDEs")
  else if(IS_ERROR(poke(pmap + sizeof(pde_t) * PDE_INDEX(KERNEL_TCB_START), (void *)buf, bufSize)))
    RET_MSG(E_FAIL, "Unable to poke kernel PDEs")

  return E_OK;
}

/**
  Zero out a page frame.

  @param phys Physical address frame to clear.
  @return E_OK on success. E_FAIL on failure.
 */

int clearPhysPage( paddr_t phys )
{
  if(IS_ERROR(mapTemp(phys)))
    RET_MSG(E_FAIL, "Unable to map temporary page address.")

  memset( (void *)TEMP_PAGEADDR, 0, PAGE_SIZE );

  unmapTemp();
  return E_OK;
}

/**
    Can data be read from some virtual address in a particular address space?

    @param addr The virtual address to be tested.
    @param pdir The physical address of the address space
    @return true if address is readable. false, otherwise.
 **/

bool isReadable( addr_t addr, paddr_t pdir )
{
  pte_t pte;

  if(IS_ERROR(readPTE( addr, &pte, pdir )))
    return false;

  return pte.present ? true : false;
}

/**
    Can data be written to some virtual address in a particular address space?

    @param addr The virtual address to be tested.
    @param pdir The physical address of the address space
    @return true if address is writable. false, otherwise.
 **/

bool isWritable( addr_t addr, paddr_t pdir )
{
  pte_t pte;
  pde_t pde;

  if(IS_ERROR(readPDE( PDE_INDEX(addr), &pde, pdir )))
    return false;

  if( !pde.rwPriv )
    return false;

  if(IS_ERROR(readPTE( addr, &pte, pdir )))
    return false;

  return (pte.present && pte.rwPriv) ? true : false;
}

/**
  Read an arbitrary entry from a generic page map in some address space.
  (A page directory is considered to be a level 2 page map, a page table
   is a level 1 page map.)

  @param pbase The base physical address of the page map. NULL_PADDR,
         if the current page map is to be used.
  @param entry The page map entry to be read.
  @param buffer The memory buffer to which the entry data will be written.
  @return E_OK on success. E_FAIL on failure. E_NOT_MAPPED if the
          PDE corresponding to the virtual address hasn't been mapped
          yet. E_INVALID_ARG if pdir is NULL_PADDR.
 */

int readPmapEntry(paddr_t pbase, int entry, void *buffer)
{
  if(pbase == NULL_PADDR)
    pbase = (paddr_t)getRootPageMap();

  return peek(pbase+sizeof(pmap_t)*entry, buffer, sizeof(pmap_t));
}

/**
  Write an arbitrary entry to a generic page map in some address space.
  (A page directory is considered to be a level 2 page map, a page table
   is a level 1 page map.)
  @param pbase The base physical address of the page map. NULL_PADDR
         if the current page map is to be used.
  @param entry The page map entry to be written.
  @param buffer The memory buffer from which the entry data will be read.
  @return E_OK on success. E_FAIL on failure. E_NOT_MAPPED if the
          PDE corresponding to the virtual address hasn't been mapped
          yet. E_INVALID_ARG if pdir is NULL_PADDR.
 */

int writePmapEntry(paddr_t pbase, int entry, void *buffer)
{
  if(pbase == NULL_PADDR)
    pbase = (paddr_t)getRootPageMap();

  return poke(pbase+sizeof(pmap_t)*entry, buffer, sizeof(pmap_t));
}

#define _readPTE(virt)  _readPTE2(virt, PAGETAB)
#define _readPTE2(virt, base) *((pmap_t *)(base + ((int)(virt)) >> 10))
#define _writePTE(virt, x) _writePTE2(virt, x, PAGETAB)
#define _writePTE2(virt, x, base)  *((pmap_t *)(base + ((int)(virt)) >> 10)) = x

/**
  Flushes the entire TLB by reloading the CR3 register.

  This must be done when switching to a new address space.
  If only a few entries need to be flushed, use invalidatePage().
 */

void invalidateTlb(void)
{
  __asm__ __volatile__("movl %%cr3, %%eax\n"
      "movl %%eax, %%cr3\n" ::: "eax", "memory");
}

/**
  Flushes a single page from the TLB.

  Instead of flushing the entire TLB, only a single page is flushed.
  This is faster than reload_cr3() for flushing a few entries.
 */

void invalidatePage( addr_t virt )
{
  __asm__ __volatile__( "invlpg (%0)\n" :: "r"( virt ) : "memory" );
}

dword getRootPageMap(void)
{
  return getCR3() & ~PCID_MASK;
}

/**
  Reads a page directory entry from an address space.

  @param entryNum The index of the PDE in the page directory
  @param pde The PDE to be read.
  @param pdir The physical address of the page directory. If
         NULL_PADDR, then use the physical address of the current
         page directory in register CR3.
  @return E_OK on success. E_FAIL on failure. E_INVALID_ARG if
         pdir is NULL_PADDR.
 */

static int readPDE( unsigned entryNum, pde_t *pde, paddr_t pdir )
{
  assert( entryNum < 1024 );
  assert( pde != NULL );

  if(pdir == NULL_PADDR)
  {
    *pde = *(pde_t *)ADDR_TO_PDE( entryNum << 22 );
    return E_OK;
  }
  else
    return readPmapEntry(pdir, entryNum, pde);
}

/**
  Reads a page table entry from a page table in mapped in an address space.

  @param virt The virtual address for which the PTE represents.
  @param pte The PTE to be read.
  @param pdir The physical address of the page directory. If
         NULL_PADDR, then use the physical address of the current
         page directory in register CR3.
  @return E_OK on success. E_FAIL on failure. E_NOT_MAPPED if the
          PDE corresponding to the virtual address hasn't been mapped
          yet. E_INVALID_ARG if pdir is NULL_PADDR
 */

int readPTE( addr_t virt, pte_t *pte, paddr_t pdir )
{
  pde_t pde;

  if(IS_ERROR(readPDE(PDE_INDEX(virt), &pde, pdir)))
    RET_MSG(E_FAIL, "Unable to read PDE")

  if( !pde.present )
    RET_MSG(E_NOT_MAPPED, "PDE is not present for virtual address.");

  if( pdir == NULL_PADDR )
  {
    *pte = *(pte_t *)ADDR_TO_PTE( virt );
    return E_OK;
  }
  else
    return readPmapEntry((paddr_t)(pde.base << PMAP_FLAG_BITS), PTE_INDEX(virt), pte);
}

/**
  Writes data to an address in physical memory.

  @param phys The starting physical address to which data will be copied.
  @param buffer The starting address from which data will be copied.
  @param bytes The number of bytes to be written.
  @return E_OK on success. E_INVALID_ARG if phys is NULL_PADDR.
 */

int poke( paddr_t phys, void *buffer, size_t bytes )
{
  return accessPhys( phys, buffer, bytes, false );
}

/**
  Reads data from an address in physical memory.

  @param phys The starting physical address from which data will be copied.
  @param buffer The starting address to which data will be copied.
  @param bytes The number of bytes to be read.
  @return E_OK on success. E_INVALID_ARG if phys is NULL_PADDR.
 */

int peek( paddr_t phys, void *buffer, size_t bytes )
{
  return accessPhys( phys, buffer, bytes, true );
}

/**
  Reads/writes data to/from an address in physical memory.

  This assumes that the memory regions don't overlap.

  @param phys The starting physical address
  @param buffer The starting address of the buffer
  @param bytes The number of bytes to be read/written.
  @param readPhys True if operation is a read, false if it's a write
  @return E_OK on success. E_INVALID_ARG if phys is NULL_PADDR.
 */

static int accessPhys( paddr_t phys, void *buffer, size_t len, bool readPhys )
{
  size_t offset;
  size_t bytes;

  if(phys == NULL_PADDR)
    RET_MSG(E_INVALID_ARG, "Invalid physical address")

  for( size_t i=0; len; phys += bytes, i += bytes, len -= bytes )
  {
    offset = (size_t)(phys & (PAGE_SIZE - 1));
    bytes = (len > PAGE_SIZE - offset) ? PAGE_SIZE - offset : len;

    mapTemp( phys & ~(PAGE_SIZE-1) );

    if( readPhys )
      memcpy( (void *)((addr_t)buffer + i), (void *)(TEMP_PAGEADDR + offset), bytes);
    else
      memcpy( (void *)(TEMP_PAGEADDR + offset), (void *)((addr_t)buffer + i), bytes);

    unmapTemp();
  }

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

static int accessMem( addr_t address, size_t len, void *buffer, paddr_t pdir, bool read )
{
  addr_t read_addr;
  addr_t write_addr;
  paddr_t read_pdir;
  paddr_t write_pdir;
  size_t addr_offset;
  size_t buffer_offset = 0;
  pte_t pte;
  size_t bytes;

  assert( buffer != NULL );
  assert( address != NULL );

  if( read )
  {
    read_addr = address;
    write_addr = (addr_t)buffer;
    read_pdir = pdir;
    write_pdir = NULL_PADDR;
  }
  else
  {
    write_addr = address;
    read_addr = (addr_t)buffer;
    write_pdir = pdir;
    read_pdir = NULL_PADDR;
  }

  for( addr_offset=0; addr_offset < len;
      addr_offset = (len - addr_offset < PAGE_SIZE ? len : addr_offset + PAGE_SIZE) )
  {
    if( !isReadable(read_addr+addr_offset, read_pdir) || !isWritable(write_addr+addr_offset, write_pdir) )
    {
#if DEBUG
      if( !isReadable(read_addr+addr_offset, read_pdir) )
        kprintf("0x%x is not readable in address space: 0x%llx\n", read_addr+addr_offset, read_pdir);

      if( !isWritable(write_addr+addr_offset, write_pdir) )
        kprintf("0x%x is not writable in address space: 0x%llx\n", write_addr+addr_offset, write_pdir);
#endif /* DEBUG */

      RET_MSG(E_FAIL, "Unable to perform memory operation.");
    }
  }

  while( len )
  {
    if( IS_ERROR(readPTE( address, &pte, pdir )) )
      RET_MSG(E_FAIL, "Unable to to PTE for address")

    addr_offset = address & (PAGE_SIZE - 1);
    bytes = (len > PAGE_SIZE - addr_offset) ? PAGE_SIZE - addr_offset : len;

    if( read )
      peek( (paddr_t)PFRAME_TO_ADDR(pte.base) + addr_offset, (void *)((addr_t)buffer + buffer_offset), bytes );
    else
      poke( (paddr_t)PFRAME_TO_ADDR(pte.base) + addr_offset, (void *)((addr_t)buffer + buffer_offset), bytes );

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

int pokeVirt( addr_t address, size_t len, void *buffer, paddr_t pdir )
{
  if( pdir == (paddr_t)getRootPageMap() )
  {
    memcpy( (void *)address, buffer, len );
    return E_OK;
  }
  else
    return accessMem( address, len, buffer, pdir, false );
}

/* data in 'address' goes to 'buffer' */

/**
  Read data from an address in an address space.

  Equivalent to an accessMem( address, len, buffer, pdir, true ).

  @param address The address in the address space to perform the read.
  @param len The length of the block to read.
  @param buffer The buffer in the current address space that is used for the read.
  @param pdir The physical address of the address space.
  @return E_OK on success. E_FAIL on failure.
 */

int peekVirt( addr_t address, size_t len, void *buffer, paddr_t pdir )
{
  if( pdir == (paddr_t)getRootPageMap() )
  {
    memcpy( buffer, (void *)address, len );
    return E_OK;
  }
  else
    return accessMem( address, len, buffer, pdir, true );
}

/**
  Maps a page in physical memory to virtual memory in the current address space

  @param virt The virtual address of the page.
  @param phys The physical address of the page.
  @param flags The flags that modify page properties.
  @return E_OK on success. E_INVALID_ARG if virt is invalid or phys is NULL.
          E_OVERWRITE if a mapping already exists.
          E_NOT_MAPPED if the page table of the page isn't mapped.
 */

int kMapPage( addr_t virt, paddr_t phys, u32 flags )
{
  pde_t *pdePtr;
  pte_t *ptePtr;

  if( virt == INVALID_VADDR || phys == NULL_PADDR )
  {
    kprintf("kMapPage(): %x -> %llx\n", virt, phys);
    RET_MSG(E_INVALID_ARG, "Invalid address.")
  }

  pdePtr = ADDR_TO_PDE( virt );

  if(IS_FLAG_SET(flags, PAGING_4MB_PAGE))
  {
    if( pdePtr->present )
    {
      kprintf("kMapPage(): %x -> %llx\n", virt, phys);
      RET_MSG(E_OVERWRITE, "Mapping already exists for virtual address.")
    }

    *(dword *)pdePtr = (dword)phys | (flags & PMAP_FLAG_MASK) | PAGING_PRES;
  }
  else
  {
    ptePtr = ADDR_TO_PTE( virt );

    if ( !pdePtr->present )
    {
      kprintf("kMapPage(): %x -> %llx\n", virt, phys);
      RET_MSG(E_NOT_MAPPED, "PDE is not present for virtual address.")
    }
    else if( ptePtr->present )
    {
      kprintf("kMapPage(): %x -> %llx\n", virt, phys);
      RET_MSG(E_OVERWRITE, "Mapping already exists for virtual address.")
    }

    *(dword *)ptePtr = (dword)phys | (flags & PMAP_FLAG_MASK) | PAGING_PRES;
  }

  invalidatePage( virt );

  return E_OK;
}

/**
  Unmaps a page from the current address space

  @param virt The virtual address of the page to unmap.
  @return E_OK on success with *phys set to the physical address of the page frame.
          E_INVALID_ARG if virt is INVALID_VADDR. E_NOT_MAPPED if address was already unmapped.
 */

int kUnmapPage( addr_t virt, paddr_t *phys )
{
  pte_t *ptePtr;
  pde_t *pdePtr;

  if( virt == INVALID_VADDR  )
    RET_MSG(E_INVALID_ARG, "Invalid virtual address.")

    pdePtr = ADDR_TO_PDE( virt );

  if (pdePtr->present)
  {
    if(!(pdePtr->pageSize))
    {
      ptePtr = ADDR_TO_PTE( virt );

      if(ptePtr->present)
      {
        if(phys)
          *phys = (paddr_t)PFRAME_TO_ADDR(ptePtr->base);

        ptePtr->present = 0;
      }
      else
        RET_MSG(E_NOT_MAPPED, "Virtual address is not mapped.")
    }
    else
    {
      if(phys)
        *phys = (paddr_t)PFRAME_TO_ADDR(pdePtr->base);

      pdePtr->present = 0;
    }
  }
  else
    RET_MSG(E_NOT_MAPPED, "PDE is not present for virtual address.")

    invalidatePage( virt );

  return E_OK;
}

/**
  Maps a page in physical memory to virtual memory in the current address space

  @param virt The virtual address of the page.
  @param phys The physical address of the page.
  @param flags The flags that modify page properties.
  @return E_OK on success. E_INVALID_ARG if virt is INVALID_VADDR or phys is NULL_PADDR.
          E_OVERWRITE if a mapping already exists.
 */

int kMapPageTable( addr_t virt, paddr_t phys, u32 flags )
{
  pde_t *pdePtr;

  if( virt == INVALID_VADDR || phys == NULL_PADDR )
  {
    kprintf("kMapPageTable(): %x -> %llx: ", virt, phys);

    if(virt == INVALID_VADDR)
      kprintf(" Invalid virtual address.");

    if(phys == NULL_PADDR)
      kprintf(" Invalid physical address.");

    kprintf("\n");

    RET_MSG(E_INVALID_ARG, "Invalid address");
  }

  pdePtr = ADDR_TO_PDE( virt );

  if( pdePtr->present )
  {
    kprintf("kMapPageTable(): %x -> %llx:\n", virt, phys);
    RET_MSG(E_OVERWRITE, "Address is already mapped!")
  }

  *(dword *)pdePtr = (dword)phys | (flags & PMAP_FLAG_MASK) | PAGING_PRES;

  return E_OK;
}

/**
  Unmaps a page table from the current address space

  @param virt The virtual address of the page.
  @param phys A pointer for the unmapped physical address of the page table that
              will be returned
  @return E_OK on success. E_INVALID_ARG if virt is INVALID_VADDR or phys is NULL_PADDR.
          E_NOT_MAPPED if mapping didn't exist yet
 */

int kUnmapPageTable( addr_t virt, paddr_t *phys )
{
  pde_t *pdePtr;

  if( virt == INVALID_VADDR)
    RET_MSG(E_INVALID_ARG, "Invalid virtual address");

  pdePtr = ADDR_TO_PDE( virt );

  if( !pdePtr->present )
    RET_MSG(E_NOT_MAPPED, "PDE is not present for virtual address.")

  pdePtr->present = 0;

  if(phys)
    *phys = (paddr_t)pdePtr->base << PMAP_FLAG_BITS;

  addr_t tableBaseAddr = ALIGN_DOWN(virt, pageTableSize);

  for(addr_t addr=tableBaseAddr; addr < tableBaseAddr + pageTableSize; addr += PAGE_SIZE)
    invalidatePage(addr);

  return E_OK;
}
