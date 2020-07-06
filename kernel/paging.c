#include <kernel/mm.h>
#include <kernel/debug.h>
#include <kernel/thread.h>
#include <kernel/memory.h>
#include <kernel/paging.h>
#include <oslib.h>

static int readPDE( unsigned entryNum, pde_t *pde, addr_t pdir );
static int readPTE( addr_t virt, pte_t *pte, addr_t pdir );

int readPmapEntry(addr_t pbase, int entry, void *buffer);
int writePmapEntry(addr_t pbase, int entry, void *buffer);

static int accessPhys( addr_t phys, void *buffer, size_t len, bool readPhys );
static int accessMem( addr_t address, size_t len, void *buffer, addr_t pdir,
    bool read );
int pokeVirt( addr_t address, size_t len, void *buffer, addr_t pdir );
int peekVirt( addr_t address, size_t len, void *buffer, addr_t pdir );

int poke( addr_t phys, void *buffer, size_t bytes );
int peek( addr_t phys, void *buffer, size_t bytes );

void invalidate_tlb(void);
void invalidate_page( addr_t virt );

bool is_readable( addr_t addr, addr_t pdir );
bool is_writable( addr_t addr, addr_t pdir );

bool is_readable( addr_t addr, addr_t pdir )
{
  pte_t pte;

  if( readPTE( addr, &pte, pdir ) != 0 )
    return false;

  if( pte.present )
    return true;
  else
    return false;
}

bool is_writable( addr_t addr, addr_t pdir )
{
  pte_t pte;
  pde_t pde;

  assert(addr != INVALID_VADDR);

  if( readPDE( PDE_INDEX(addr), &pde, pdir ) != 0 )
    return false;

  if( !pde.rwPriv )
    return false;

  if( readPTE( addr, &pte, pdir ) != 0 )
    return false;

  if( pte.present && pte.rwPriv )
    return true;
  else
    return false;
}

int readPmapEntry(addr_t pbase, int entry, void *buffer)
{
  return peek(pbase+sizeof(pmap_t)*entry, buffer, 4);
}

int writePmapEntry(addr_t pbase, int entry, void *buffer)
{
  return poke(pbase+sizeof(pmap_t)*entry, buffer, 4);
}

#define _readPTE(virt)  _readPTE2(virt, PAGETAB)
#define _readPTE2(virt, base) *((pmap_t *)(base + ((int)(virt)) >> 10))
#define _writePTE(virt, x) _writePTE2(virt, x, PAGETAB)
#define _writePTE2(virt, x, base)  *((pmap_t *)(base + ((int)(virt)) >> 10)) = x

/**
  Flushes the entire TLB by reloading the CR3 register.

  This must be done when switching to a new address space.
  If only a few entries need to be flushed, use invalidate_page().
*/

inline void invalidate_tlb(void)
{
  __asm__ __volatile__("movl %%cr3, %%eax\n"
          "movl %%eax, %%cr3\n" ::: "eax", "memory");
}

/**
  Flushes a single page from the TLB.

  Instead of flushing the entire TLB, only a single page is flushed.
  This is faster than reload_cr3() for flushing a few entries.
*/

inline void invalidate_page( addr_t virt )
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
  @return 0 on success. -1 on failure.
*/

static int readPDE( unsigned entryNum, pde_t *pde, addr_t pdir )
{
  assert( entryNum < 1024 );
  assert( pde != NULL );

  if(pdir == NULL_PADDR)
  {
    *pde = *(pde_t *)ADDR_TO_PDE( entryNum << 22 );
    return 0;
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
  @return 0 on success. -1 on failure.
*/

int readPTE( addr_t virt, pte_t *pte, addr_t pdir )
{
  pde_t pde;

  if( readPDE( PDE_INDEX(virt), &pde, pdir ) != 0 )
  {
    kprintf("readPDE() failed\n");
    return -1;
  }

  if( !pde.present )
    return -1;

  if( pdir == NULL_PADDR )
  {
    *pte = *(pte_t *)ADDR_TO_PTE( virt );
    return 0;
  }
  else
    return readPmapEntry((addr_t)(pde.base << 12), PTE_INDEX(virt), pte);
}

/**
  Writes data to an address in physical memory.

  @param phys The starting physical address to which data will be copied.
  @param buffer The starting address from which data will be copied.
  @param bytes The number of bytes to be written.
  @return 0 on success. -1 on failure.
*/

int poke( addr_t phys, void *buffer, size_t bytes )
{
  return accessPhys( phys, buffer, bytes, false );
}

/**
  Reads data from an address in physical memory.

  @param phys The starting physical address from which data will be copied.
  @param buffer The starting address to which data will be copied.
  @param bytes The number of bytes to be read.
  @return 0 on success. -1 on failure.
*/

int peek( addr_t phys, void *buffer, size_t bytes )
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
  @return 0 on success. -1 on failure.
*/

static int accessPhys( addr_t phys, void *buffer, size_t len, bool readPhys )
{
  size_t offset, bytes;

  if(phys == NULL_PADDR)
    return -1;

  for( size_t i=0; len; phys += bytes, i += bytes, len -= bytes )
  {
    offset = (size_t)phys & (PAGE_SIZE - 1);
    bytes = (len > PAGE_SIZE - offset) ? PAGE_SIZE - offset : len;

    mapTemp( phys & ~(PAGE_SIZE-1) );

    if( readPhys )
      memcpy( (void *)((addr_t)buffer + i), (void *)(TEMP_PAGEADDR + offset), bytes);
    else
      memcpy( (void *)(TEMP_PAGEADDR + offset), (void *)((addr_t)buffer + i), bytes);

    unmapTemp();
  }

  return 0;
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
  @return 0 on success. -1 on failure.
*/

static int accessMem( addr_t address, size_t len, void *buffer, addr_t pdir, bool read )
{
  addr_t read_addr;
  addr_t write_addr;
  addr_t read_pdir;
  addr_t write_pdir;
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
    if( !is_readable(read_addr+addr_offset, read_pdir) || !is_writable(write_addr+addr_offset, write_pdir) )
    {
      #if DEBUG
        if( !is_readable(read_addr+addr_offset, read_pdir) )
          kprintf("0x%x is not readable in address space: 0x%x\n", read_addr+addr_offset, read_pdir);

        if( !is_writable(write_addr+addr_offset, write_pdir) )
          kprintf("0x%x is not writable in address space: 0x%x\n", write_addr+addr_offset, write_pdir);
      #endif /* DEBUG */

      return -1;
    }
  }

  while( len )
  {
    if( readPTE( address, &pte, pdir ) != 0 )
      return -1;

    addr_offset = address & (PAGE_SIZE - 1);
    bytes = (len > PAGE_SIZE - addr_offset) ? PAGE_SIZE - addr_offset : len;

    if( read )
      peek( ((addr_t)pte.base << 12) + addr_offset, (void *)((addr_t)buffer + buffer_offset), bytes );
    else
      poke( ((addr_t)pte.base << 12) + addr_offset, (void *)((addr_t)buffer + buffer_offset), bytes );

    address += bytes;
    buffer_offset += bytes;
    len -= bytes;
  }

  return 0;
}

/**
  Write data to an address in an address space.

  Equivalent to an accessMem( address, len, buffer, pdir, false ).

  @param address The address in the address space to perform the write.
  @param len The length of the block to write.
  @param buffer The buffer in the current address space that is used for the write.
  @param pdir The physical address of the address space.
  @return 0 on success. -1 on failure.
*/

int pokeVirt( addr_t address, size_t len, void *buffer, addr_t pdir )
{
  if( pdir == (addr_t)(getCR3() & ~(PAGE_SIZE-1)) )
  {
    memcpy( (void *)address, buffer, len );
    return 0;
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
  @return 0 on success. -1 on failure.
*/

int peekVirt( addr_t address, size_t len, void *buffer, addr_t pdir )
{
  if( pdir == (addr_t)(getCR3() & ~(PAGE_SIZE-1)) )
  {
    memcpy( buffer, (void *)address, len );
    return 0;
  }
  else
    return accessMem( address, len, buffer, pdir, true );
}

/**
  Maps a page in physical memory to virtual memory in the current address space

  @param virt The virtual address of the page.
  @param phys The physical address of the page.
  @param flags The flags that modify page properties.
  @return 0 on success. -1 on failure. -2 if a mapping already exists.
          -3 if the page table of the page isn't mapped.
*/

int kMapPage( addr_t virt, addr_t phys, u32 flags )
{
  pde_t *pdePtr;
  pte_t *ptePtr;

  assert( virt != INVALID_VADDR );
  assert( phys != NULL_PADDR );

  if( virt == INVALID_VADDR || phys == NULL_PADDR )
  {
    kprintf("mapPage(): Error!!! ");
    return -1;
  }

  assert( phys == (phys & ~0xFFF) );
  assert( virt == (virt & ~0xFFF) );

  pdePtr = ADDR_TO_PDE( virt );
  ptePtr = ADDR_TO_PTE( virt );

  if ( !pdePtr->present )
  {
    kprintf("Trying to map %x -> %x, but no PDE present.\n", phys, virt);
    assert( false );
    return -3;
  }
  else if( ptePtr->present )
  {
    kprintf("%x -> %x: 0x%x is already mapped!\n", virt, phys, virt);
    assert( false );
    return -2;
  }

  *(dword *)ptePtr = (dword)phys | (flags & 0xFFFu) | PAGING_PRES;
  invalidate_page( virt );

  return 0;
}

/**
  Unmaps a page from the current address space

  @param virt The virtual address of the page to unmap.
  @return 0 on success with *phys set to the physical address of the page frame. -1 on failure.
*/

int kUnmapPage( addr_t virt, addr_t *phys )
{
  pte_t *ptePtr;
  pde_t *pdePtr;

  assert( virt == (virt & ~0xFFF) );

  ptePtr = ADDR_TO_PTE( virt );
  pdePtr = ADDR_TO_PDE( virt );

  assert( pdePtr->present && ptePtr->present );

  if ( pdePtr->present && ptePtr->present )
  {
    if(phys != NULL)
      *phys = (addr_t)ptePtr->base << 12;

    ptePtr->present = 0;
    invalidate_page( virt );
  }
  else
    return -1;

  return 0;
}
