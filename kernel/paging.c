#include <kernel/mm.h>
#include <kernel/debug.h>
#include <kernel/thread.h>
#include <kernel/memory.h>
#include <kernel/paging.h>
#include <kernel/error.h>
#include <oslib.h>
#include <string.h>

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

int initializeRootPmap(dword pmap)
{
 // Map the page directory, kernel space, and first page table
 // into the new address space

  u32 pentry = (u32)pmap | PAGING_RW | PAGING_PRES;

  // recursively map the page map to itself

  if(writePmapEntry(pmap, PDE_INDEX(PAGETAB), &pentry) != E_OK)
    return E_FAIL;
#if DEBUG

  // map the first page table

  if(writePmapEntry(pmap, 0, (void *)PAGEDIR) != E_OK)
    return E_FAIL;
#endif /* DEBUG */

  // Copy any page tables in the kernel region of virtual memory from
  // the bootstrap address space to the thread's address space

  size_t bufSize = 4*(1023-PDE_INDEX(KERNEL_TCB_START));
  void *buf = malloc(bufSize);

  if(!buf)
    return E_FAIL;
  else if(peek(initKrnlPDir+4*PDE_INDEX(KERNEL_TCB_START), buf, bufSize) != E_OK
     || poke(pmap+4*PDE_INDEX(KERNEL_TCB_START), buf, bufSize) != E_OK)
  {
    free(buf);
    return E_FAIL;
  }

  free(buf);

  return E_OK;
}

/**
  Zero out a page frame.

  @param phys Physical address frame to clear.
  @return E_OK on success. E_FAIL on failure.
*/

int clearPhysPage( paddr_t phys )
{
  if( mapTemp( phys ) != E_OK )
    return E_FAIL;

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

  if( readPTE( addr, &pte, pdir ) != E_OK )
    return false;

  if( pte.present )
    return true;
  else
    return false;
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

  if( readPDE( PDE_INDEX(addr), &pde, pdir ) != E_OK )
    return false;

  if( !pde.rwPriv )
    return false;

  if( readPTE( addr, &pte, pdir ) != E_OK )
    return false;

  if( pte.present && pte.rwPriv )
    return true;
  else
    return false;
}

/**
  Read an arbitrary entry from a generic page map in some address space.
  (A page directory is considered to be a level 2 page map, a page table
   is a level 1 page map.)
  @param pbase The base physical address of the page map
  @param entry The page map entry to be read.
  @param buffer The memory buffer to which the entry data will be written.
  @return E_OK on success. E_FAIL on failure. E_NOT_MAPPED if the
          PDE corresponding to the virtual address hasn't been mapped
          yet. E_INVALID_ARG if pdir is NULL_PADDR.
*/

int readPmapEntry(paddr_t pbase, int entry, void *buffer)
{
  return peek(pbase+sizeof(pmap_t)*entry, buffer, sizeof(pmap_t));
}

/**
  Write an arbitrary entry to a generic page map in some address space.
  (A page directory is considered to be a level 2 page map, a page table
   is a level 1 page map.)
  @param pbase The base physical address of the page map
  @param entry The page map entry to be written.
  @param buffer The memory buffer from which the entry data will be read.
  @return E_OK on success. E_FAIL on failure. E_NOT_MAPPED if the
          PDE corresponding to the virtual address hasn't been mapped
          yet. E_INVALID_ARG if pdir is NULL_PADDR.
*/

int writePmapEntry(paddr_t pbase, int entry, void *buffer)
{
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

  if( readPDE( PDE_INDEX(virt), &pde, pdir ) != E_OK )
  {
    kprintf("readPDE() failed\n");
    return E_FAIL;
  }

  if( !pde.present )
    return E_NOT_MAPPED;

  if( pdir == NULL_PADDR )
  {
    *pte = *(pte_t *)ADDR_TO_PTE( virt );
    return E_OK;
  }
  else
    return readPmapEntry((paddr_t)(pde.base << 12), PTE_INDEX(virt), pte);
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
  size_t offset, bytes;

  if(phys == NULL_PADDR)
    return E_INVALID_ARG;

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
  size_t buffer_offset;
  pte_t pte;
  size_t bytes;

  assert( buffer != NULL );
  assert( address != NULL );

  if( read )
  {
    read_addr = address;
    write_addr = (addr_t)buffer;
    read_pdir = pdir;
    write_pdir = NULL_PADDR; //getCR3() & ~0xFFFu;
  }
  else
  {
    write_addr = address;
    read_addr = (addr_t)buffer;
    write_pdir = pdir;
    read_pdir = NULL_PADDR; //getCR3() & ~0xFFFu;
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

      return E_FAIL;
    }
  }

  while( len )
  {
    if( readPTE( address, &pte, pdir ) != E_OK )
      return E_FAIL;

    addr_offset = address & (PAGE_SIZE - 1);
    bytes = (len > PAGE_SIZE - addr_offset) ? PAGE_SIZE - addr_offset : len;

    if( read )
      peek( ((paddr_t)pte.base << 12) + addr_offset, (void *)((addr_t)buffer + buffer_offset), bytes );
    else
      poke( ((paddr_t)pte.base << 12) + addr_offset, (void *)((addr_t)buffer + buffer_offset), bytes );

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
  if( pdir == (paddr_t)(getCR3() & ~(PAGE_SIZE-1)) )
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
  if( pdir == (paddr_t)(getCR3() & ~(PAGE_SIZE-1)) )
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
    kprintf("kMapPage(): %llx -> %x: ", phys, virt);

    if(virt == INVALID_VADDR)
      kprintf(" Invalid virtual address.");

    if(phys == NULL_PADDR)
      kprintf(" Invalid physical address.");

    kprintf("\n");

    return E_INVALID_ARG;
  }

  pdePtr = ADDR_TO_PDE( virt );

  if(flags & PAGING_4MB_PAGE)
  {
    if( pdePtr->present )
    {
      kprintf("kMapPage(): %llx -> %x: 0x%x is already mapped!\n", phys, virt, virt);
      return E_OVERWRITE;
    }

    *(dword *)pdePtr = (dword)phys | (flags & 0xFFFu) | PAGING_PRES;
  }
  else
  {
    ptePtr = ADDR_TO_PTE( virt );

    if ( !pdePtr->present )
    {
      kprintf("kMapPage(): Trying to map %llx -> %x, but no PDE present.\n", phys, virt);
      return E_NOT_MAPPED;
    }
    else if( ptePtr->present )
    {
      kprintf("kMapPage(): %llx -> %x: 0x%x is already mapped!\n", phys, virt, virt);
      return E_OVERWRITE;
    }

    *(dword *)ptePtr = (dword)phys | (flags & 0xFFFu) | PAGING_PRES;
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
  {
    kprintf("kUnmapPage(): Invalid virtual address.\n");

    return E_INVALID_ARG;
  }

  pdePtr = ADDR_TO_PDE( virt );

  if ( pdePtr->present )
  {
    if(!(pdePtr->pageSize))
    {
      ptePtr = ADDR_TO_PTE( virt );

      if(ptePtr->present)
      {
        if(phys != NULL)
          *phys = (paddr_t)ptePtr->base << 12;

        ptePtr->present = 0;
      }
      else
        return E_NOT_MAPPED;
    }
    else
    {
      if(phys != NULL)
        *phys = (paddr_t)pdePtr->base << 12;

      pdePtr->present = 0;
    }
  }
  else
    return E_NOT_MAPPED;

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
    kprintf("kMapPageTable(): %llx -> %x: ", phys, virt);

    if(virt == INVALID_VADDR)
      kprintf(" Invalid virtual address.");

    if(phys == NULL_PADDR)
      kprintf(" Invalid physical address.");

    kprintf("\n");

    return E_INVALID_ARG;
  }

  pdePtr = ADDR_TO_PDE( virt );

  if( pdePtr->present )
  {
    kprintf("kMapPageTable(): %llx -> %x: 0x%x is already mapped!\n", phys, virt, virt);
    return E_OVERWRITE;
  }

  *(dword *)pdePtr = (dword)phys | (flags & 0xFFFu) | PAGING_PRES;

  invalidatePage(virt & ~(PAGE_TABLE_SIZE-1));

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
  {
    kprintf("mapPageTable(): Invalid virtual address.\n");

    return E_INVALID_ARG;
  }

  pdePtr = ADDR_TO_PDE( virt );

  if( !pdePtr->present )
    return E_NOT_MAPPED;

  pdePtr->present = 0;

  if(phys != NULL)
    *phys = (paddr_t)pdePtr->base << 12;

  invalidatePage(virt & ~(PAGE_TABLE_SIZE-1));

  return E_OK;
}
