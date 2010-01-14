#include <kernel/mm.h>
#include <oslib.h>
#include <kernel/debug.h>
#include <kernel/thread.h>
#include <kernel/memory.h>

int _writePDE( unsigned entryNum, pde_t *pde, void * addrSpace );
int _readPDE( unsigned entryNum, pde_t *pde, void * addrSpace );
int readPDE( void * virt, pde_t *pde, void * addrSpace );
int readPTE( void * virt, pte_t *pte, void * addrSpace );
int writePTE( void * virt, pte_t *pte, void * addrSpace );
int writePDE( void * virt, pde_t *pde, void * addrSpace );

//int clearPhysPage( void *phys );
int accessMem( void *address, size_t len, void *buffer, void *addrSpace,
    bool read );
int pokeMem( void *address, size_t len, void *buffer, void *addrSpace );
int peekMem( void *address, size_t len, void *buffer, void *addrSpace );
//addr_t getPhysAddr( void *addr, void *addrSpace );
int mapPageTable( void *phys, void *virt, u32 flags, 
     void *addrSpace );
int mapPage( void *phys, void *virt, u32 flags, void *addrSpace );
addr_t unmapPageTable( void *virt, void *addrSpace );
addr_t unmapPage( void *virt, void *addrSpace );

inline void reload_cr3(void) __attribute__((always_inline));
inline void invalidate_page( void *virt ) __attribute__((always_inline));

/**
  Flushes the entire TLB by reloading the CR3 register.

  This must be done when switching to a new address space.
  If only a few entries need to be flushed, use invalidate_page().
*/

inline void reload_cr3(void)
{
  __asm__ volatile("push %eax\n"
                   "movl %cr3, %eax\n"
                   "movl %eax, %cr3\n"
                   "pop %eax\n" );
}

/** 
  Flushes a single page from the TLB.

  Instead of flushing the entire TLB, only a single page is flushed.
  This is faster than reload_cr3() for flushing a few entries.
*/

inline void invalidate_page( void *virt )
{
  __asm__ volatile( "invlpg %0\n" :: "m"( *(char *)virt ) );
}

/**
  Writes a page directory entry into an address space

  @note When modifying a page directory entry in kernel space, all
  page directories need to be accessible and modified.

  @param entryNum The index of the PDE in the page directory.
  @param pde The PDE to be written.
  @param addrSpace The physical address of the page directory.
  @return 0 on success. -1 on failure.
*/

int _writePDE( unsigned entryNum, pde_t *pde, void *addrSpace )
{
  pdir_t *dir = (pdir_t *)(TEMP_PAGEADDR );
  pde_t *pdePtr;

  assert( entryNum < 1024 );
  assert( pde != NULL );
  assert(addrSpace != (void *)NULL_PADDR);

  if( entryNum >= 1024 || pde == NULL || addrSpace == NULL_PADDR )
    return -1;

  if( addrSpace == (addr_t)getCR3() )
  {
    pdePtr = (pde_t *)ADDR_TO_PDE( (addr_t)(entryNum << 22) );
    *pdePtr = *pde;
  }
  else
  {
    if( mapTemp( addrSpace ) == -1 )
    {
      assert(false);
      return -1;
    }

    dir->pageTables[entryNum] = *pde;

    unmapTemp( );
  }
  return 0;
}

/** 
  Writes a page directory entry into an address space.

  @param virt The virtual address for which the PDE will represent.
  @param pde The PDE to be written.
  @param addrSpace The physical address of the page directory.
  @return 0 on success. -1 on failure.
*/

int writePDE( void *virt, pde_t *pde, void *addrSpace )
{
  assert( virt != (void *)INVALID_VADDR );
  assert( addrSpace != (void *)NULL_PADDR );

  if( virt == (void *)INVALID_VADDR || addrSpace == NULL_PADDR )
    return -1;

  return _writePDE( (unsigned)virt >> 22, pde, addrSpace );
}

/**
  Reads a page directory entry from an address space.

  @param entryNum The index of the PDE in the page directory
  @param pde The PDE to be read.
  @param addrSpace The physical address of the page directory.
  @return 0 on success. -1 on failure.
*/

int _readPDE( unsigned entryNum, pde_t *pde, void *addrSpace )
{
  pdir_t *dir = (pdir_t *)(TEMP_PAGEADDR );

  assert( entryNum < 1024 );
  assert( pde != NULL );
  assert( addrSpace != (void *)NULL_PADDR );

  if( entryNum >= 1024 || pde == NULL || addrSpace == NULL_PADDR )
    return -1;

  if( addrSpace == (addr_t)getCR3() )
  {
    *pde = *(pde_t *)ADDR_TO_PDE( (addr_t)(entryNum << 22) );
    return 0;
  }

  if( mapTemp( addrSpace ) == -1 )
    return -1;

  *pde = dir->pageTables[entryNum];

  unmapTemp( );

  return 0;
}

/** 
  Reads a page directory entry from an address space.

  @param virt The virtual address for which the PDE represents.
  @param pde The PDE to be read.
  @param addrSpace The physical address of the page directory.
  @return 0 on success. -1 on failure.
*/

int readPDE( void *virt, pde_t *pde, void *addrSpace )
{
    assert( virt != (void *)INVALID_VADDR );
    assert( pde != NULL );
    assert( addrSpace != (void *)NULL_PADDR );

    if( virt == (void *)INVALID_VADDR || pde == (void *)INVALID_VADDR || addrSpace == NULL_PADDR )
    {
         kprintf("getPDE(): virt error ");
         return -1;
    }

    return _readPDE( (unsigned)virt >> 22, pde, addrSpace );
}

/**
  Reads a page table entry from a page table in mapped in an address space.

  @param virt The virtual address for which the PTE represents.
  @param pte The PTE to be read.
  @param addrSpace The physical address of the page directory.
  @return 0 on success. -1 on failure.
*/

int readPTE( void *virt, pte_t *pte, void *addrSpace )
{
  pde_t pde;
  ptab_t *table = (ptab_t *)(TEMP_PAGEADDR );
  unsigned dirEntryNum = (unsigned)virt >> 22;
  unsigned tableEntryNum = ((unsigned)virt >> 12) & 0x3FF;
/*
  This is commented out because it fixes the message passing code. This
  means that the address space doesn't have it's page directory mapped
  in itself for user-created kernel threads. 

  if( (unsigned)addrSpace == (unsigned)getCR3() )
  {
    *pte = *(pte_t *)ADDR_TO_PTE( virt );
    return 0;
  }
*/
  assert( virt != INVALID_VADDR );
  assert( addrSpace != (void *)NULL_PADDR );
  assert( pte != INVALID_VADDR );

  if( _readPDE( dirEntryNum, &pde, addrSpace ) == -1 )
  {
    kprintf("_readPDE() failed\n");
    return -1;
  }

  if( !pde.present )
  {
    kprintf("Error! - 0x%x 0x%x 0x%x\n", virt, pte, addrSpace);
    assert(false);
    return -1;
  }

  if( (unsigned)addrSpace == (unsigned)getCR3() )
  {
    *pte = *(pte_t *)ADDR_TO_PTE( virt );
    return 0;
  }

  mapTemp( (addr_t)(pde.base << 12) );

  *pte = table->pages[tableEntryNum];

  unmapTemp();

  return 0;
}

/**
  Writes a page table entry into a page table in mapped in an address space.

  @param virt The virtual address for which the PTE will represent.
  @param pte The PTE to be written.
  @param addrSpace The physical address of the page directory.
  @return 0 on success. -1 on failure.
*/

int writePTE( void *virt, pte_t *pte, void *addrSpace )
{
  pde_t pde;
  pte_t *ptePtr;
  ptab_t *table = (ptab_t *)(TEMP_PAGEADDR );
  unsigned dirEntryNum = (unsigned)virt >> 22;
  unsigned tableEntryNum = ((unsigned)virt >> 12) & 0x3FF;

  assert( virt != INVALID_VADDR );
  assert( addrSpace != (void *)NULL_PADDR );
  assert( pte != INVALID_VADDR );

  if( _readPDE( dirEntryNum, &pde, addrSpace ) == -1 )
  {
    assert(false);
    return -1;
  }

  if( !pde.present )
  {
    kprintf("Virt: 0x%x PTE: 0x%x AddrSpace: 0x%x\n", virt, pte, addrSpace);
    assert(false);
    return -1;
  }

  if( (unsigned)addrSpace == (unsigned)getCR3() )
  {
    ptePtr = (pte_t *)ADDR_TO_PTE( virt );
    *ptePtr = *pte;
    invalidate_page( virt );
//    __asm__ volatile( "invlpg %0\n" :: "m"( *(char *)virt ) );
  }
  else
  {
    mapTemp( (addr_t)(pde.base << 12) );
    table->pages[tableEntryNum] = *pte;
    unmapTemp();
  }
  return 0;
}

/**
  Allows for reading/writing a block of memory according to the address space

  This function is used to read/write data to/from any address space. If reading,
  read len bytes from address into buffer. If writing, write len bytes from
  buffer to address.

  @param address The address in the address space to perform the read/write.
  @param len The length of the block to read/write.
  @param buffer The buffer in the current address space that is used for the read/write.
  @param addrSpace The physical address of the address space.
  @param read True if reading. False if writing.
  @return 0 on success. -1 on failure.
*/

int accessMem( void *address, size_t len, void *buffer, void *addrSpace, bool read )
{
  pte_t pte;
  unsigned offset;
  size_t bytes;
  unsigned i=0;

  assert( addrSpace != NULL_PADDR );
  assert( buffer != NULL );

  if( addrSpace == NULL_PADDR || buffer == NULL )
    return -1;

  while( len > 0 )
  {
    if( readPTE( address, &pte, addrSpace ) != 0 )
      return -1;

    offset = (unsigned)address & (PAGE_SIZE - 1);
    bytes = (len > PAGE_SIZE - offset) ? PAGE_SIZE - offset : len;

    mapTemp( (void *)(pte.base << 12) );

    if( read )
      memcpy( (void *)((unsigned)buffer + i), (void *)(TEMP_PAGEADDR + offset), bytes);
    else
      memcpy( (void *)(TEMP_PAGEADDR + offset), (void *)((unsigned)buffer + i), bytes );

    unmapTemp();

    address = (void *)((unsigned)address + bytes);
    i += bytes;
    len -= bytes;
  }
  return 0;
}

/**
  Write data to an address in an address space.

  Equivalent to an accessMem( address, len, buffer, addrSpace, false ).

  @param address The address in the address space to perform the write.
  @param len The length of the block to write.
  @param buffer The buffer in the current address space that is used for the write.
  @param addrSpace The physical address of the address space.
  @return 0 on success. -1 on failure.
*/

int pokeMem( void *address, size_t len, void *buffer, void *addrSpace )
{
  if( (unsigned)addrSpace == getCR3() )
  {
    memcpy( address, buffer, len );
    return 0;
  }
  else
    return accessMem( address, len, buffer, addrSpace, false );
}

/* data in 'address' goes to 'buffer' */

/**
  Read data from an address in an address space.

  Equivalent to an accessMem( address, len, buffer, addrSpace, true ).

  @param address The address in the address space to perform the read.
  @param len The length of the block to read.
  @param buffer The buffer in the current address space that is used for the read.
  @param addrSpace The physical address of the address space.
  @return 0 on success. -1 on failure.
*/

int peekMem( void *address, size_t len, void *buffer, void *addrSpace )
{
  if( (unsigned)addrSpace == getCR3() )
  {
    memcpy( buffer, address, len );
    return 0;
  }
  else
    return accessMem( address, len, buffer, addrSpace, true );
}

/*
 Unused

addr_t getPhysAddr( void *virt, void *addrSpace )
{
    pte_t pte;
    addr_t addr;

    if( readPTE( virt, &pte, addrSpace ) == -1 )
      return INVALID_VADDR;

    addr = (addr_t)(pte.base << 12);

    return addr;
}
*/

/**
   Maps a page table into a page direcotry.

   @param virt The virtual address of the page table.
   @param phys The physical address of the page table.
   @param flags The flags that modify memory properties.
   @param addrSpace The physical address of the page directory.
   @return 0 on success. -1 on failure. -2 if a mapping already exists.
*/

int mapPageTable( void *virt, void *phys, u32 flags, void *addrSpace )
{
  pde_t pde, * pdePtr = &pde;
  addr_t newPage, currPdir;

  assert( virt != (void *)INVALID_VADDR );
  assert( phys != (void *)NULL_PADDR );

  if( virt == (void *)INVALID_VADDR || phys == (void *)NULL_PADDR )
    RET_MSG(-1, "NULL ptr")//return -1;

  currPdir = (void *)getCR3();

  if ( addrSpace == currPdir )
    pdePtr = ADDR_TO_PDE( virt );
  else
  {
    if( readPDE( virt, pdePtr, addrSpace ) == -1 )
    {
      assert(false);
      return -1;
    }
  }

//  clearPhysPage( phys );

  if ( !pdePtr->present )
  {
    *(u32 *)pdePtr = (u32)phys | flags | 1;
    writePDE( virt, pdePtr, addrSpace );

    if( addrSpace == currPdir )
    {
// Invalidate all of the pages in the table
// It would be faster to reload cr3

    reload_cr3();

    }
  }
  else
  {
    kprintf("Already mapped 0x%x in address space 0x%x", virt, addrSpace);
//    kprintfHex(*(u32 *)pdePtr);
//    kprintf(" ");

    assert(false);
    return -2;
  }
  return 0;
}

/** 
  Maps a page in physical memory to virtual memory in the current address space

  @param virt The virtual address of the page.
  @param phys The physical address of the page.
  @param flags The flags that modify page properties.
  @return 0 on success. -1 on failure. -2 if a mapping already exists.
          -3 if the page table of the page isn't mapped.
*/

int kMapPage( void *virt, void *phys, u32 flags)
{
  pde_t *pdePtr;
  pte_t *ptePtr;

  assert( virt != (void *)INVALID_VADDR );
  assert( phys != (void *)NULL_PADDR );

  if( virt == (void *)INVALID_VADDR || 
      phys == (void *)NULL_PADDR )
  {
    kprintf("mapPage(): Error!!! ");
    return -1;
  }

  if( (u32)phys != ((u32)phys & ~0xFFF))
    kprintf("phys: 0x%x\n", phys);

  assert( (u32)phys == ((u32)phys & ~0xFFF) );

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

  *(u32 *)ptePtr = (u32)phys | flags | 1;
  invalidate_page( virt );

  return 0;
}

/* This is supposed to map a virtual address to a physical address in any
   address space. */

/** Map a page in physical memory to virtual memory in an address space
  @param virt The virtual address of the page.
  @param phys The physical address of the page.
  @param flags The flags that modify page properties.
  @param addrSpace The physical address of the page directory to be used for the mapping.
  @return 0 on success. -1 on failure. -2 if a mapping already exists.
          -3 if the page table of the page isn't mapped.
*/

int mapPage( void *virt, void *phys, u32 flags, void *addrSpace )
{
  pde_t pde, * pdePtr = &pde;
  pte_t pte, * ptePtr = &pte;
  addr_t newPage, currPdir;

  assert( virt != (void *)INVALID_VADDR );
 // assert( virt != NULL );
  assert( phys != (void *)NULL_PADDR );

  assert( (u32)phys == ((u32)phys & ~0xFFF));

  if( (u32)phys != ((u32)phys & ~0xFFF) )
    kprintf("virt: 0x%x phys 0x%x\n");

  if( virt == (void *)INVALID_VADDR || phys == (void *)NULL_PADDR )
  {
    kprintf("mapPage(): Error!!! ");
    return -1;
  }

  currPdir = (void *)getCR3();

  if ( addrSpace == currPdir )
  {
    pdePtr = ADDR_TO_PDE( virt );
    ptePtr = ADDR_TO_PTE( virt );
  }
  else
  {
    if( readPDE( virt, pdePtr, addrSpace ) == -1 )
    {
      assert(false);
      return -1;
    }
  }

  if ( !pdePtr->present )
  {
    assert( false );  /* Since page allocation isn't done in the kernel,
                         allocating a new page table needs to be done
                         manually. */
    return -3;
  }

  if( currPdir != addrSpace )
  {
    if( readPTE( virt, ptePtr, addrSpace ) < 0 )
    {
      assert(false);
      return -1;
    }
  }

  if( ptePtr->present )
  {
    kprintf("Already mapped! 0x%x->0x%x\n", phys, virt);
    assert( false );

    return -2;
  }

  *(u32 *)ptePtr = (u32)phys | flags | 1;

  if ( currPdir == addrSpace )
  {
    invalidate_page( virt );
    assert( *(u32 *)ptePtr & 0x01 );
  }
  else
    writePTE( virt, ptePtr, addrSpace );

  return 0;
}

/** 
  Unmaps a page from the current address space

  @param virt The virtual address of the page to unmap.
  @return The physical address of the unmapped page on success. NULL_PADDR on failure.
*/

addr_t kUnmapPage( void *virt )
{
  pte_t *ptePtr;
  pde_t *pdePtr;
  unsigned returnAddr;
/*
  assert( virt != (void *)INVALID_VADDR );

  if( virt == (void *)INVALID_VADDR )
  {
    kprintf("unmapPage() : Error!!! ");
    return (addr_t)NULL_PADDR;
  }
*/
  ptePtr = ADDR_TO_PTE( virt );
  pdePtr = ADDR_TO_PDE( virt );

  assert( pdePtr->present && ptePtr->present );

  if ( pdePtr->present && ptePtr->present )
  {
    returnAddr = (unsigned)(ptePtr->base << 12);
    ptePtr->present = 0;
    invalidate_page( virt );

    return (addr_t)returnAddr;
  }
  else
    return (addr_t)NULL_PADDR;
}

/** 
  Unmaps a page from an address space

  @param virt The virtual address of the page to unmap.
  @param addrSpace The physical address of the page directory.
  @return The physical address of the unmapped page on success. NULL_PADDR on failure.
*/
addr_t unmapPage( void *virt, void *addrSpace )
{
    pte_t pte, * ptePtr = &pte;
    pde_t pde, * pdePtr = &pde;
    addr_t returnAddr;

    assert( virt != (void *)INVALID_VADDR );
    assert( addrSpace != (void *)NULL_PADDR );

    if( virt == (void *)INVALID_VADDR || addrSpace == (void *)NULL_PADDR )
    {
      kprintf("unmapPage() : Error!!! ");
      return (addr_t)NULL_PADDR;
    }

    if ( addrSpace == ( addr_t ) getCR3() )
    {
	ptePtr = ADDR_TO_PTE( virt );
	pdePtr = ADDR_TO_PDE( virt );

        assert( pdePtr->present && ptePtr->present );

	if ( pdePtr->present && ptePtr->present )
	{
  	  ptePtr->present = 0;
          returnAddr = ( addr_t ) ( ptePtr->base << 12 );

          invalidate_page( virt );
//	  __asm__ volatile( "invlpg %0\n" :: "m" ( *(char *)virt ) );
	}
	else
          returnAddr = (addr_t)NULL_PADDR;
    }
    else
    {
        if( readPTE( virt, ptePtr, addrSpace ) == 0 && ptePtr->present )
        {
            ptePtr->present = 0;
            returnAddr = ( addr_t )( ptePtr->base << 12 );

            if( writePTE( virt, ptePtr, addrSpace ) == -1 )
            {
              assert(false);
              returnAddr = (addr_t)NULL_PADDR;
            }
        }
        else
        {
          kprintf("unmapPage() : Not present or readPTE() failed.");
          assert(false);
          returnAddr = (addr_t)NULL_PADDR;
        }
    }
    return returnAddr;
}

/** Unmaps a page table from an address space
  @param virt The virtual address of the page table to unmap.
  @param addrSpace The physical address of the page directory.
  @return The physical address of the unmapped page table on success. NULL_PADDR on failure.
*/
addr_t unmapPageTable( void *virt, void *addrSpace )
{
    pde_t pde, *pdePtr = &pde;
    addr_t returnAddr;

    assert( virt != (void *)INVALID_VADDR );
 //   assert( virt != NULL );
    assert( addrSpace != (void *)NULL_PADDR );

    if( /*virt == NULL || */virt == (void *)INVALID_VADDR || addrSpace == (void *)NULL_PADDR )
    {
      kprintf("unmapPage() : Error!!! ");
      return (addr_t)NULL_PADDR;
    }

    if ( addrSpace == ( addr_t ) getCR3() )
    {
        pdePtr = ADDR_TO_PDE( virt );
        returnAddr = (addr_t)(pdePtr->base << 12);
        pdePtr->present = 0;

     // Invalidate all of the 1024 pages?

// It would be faster to reload cr3

      reload_cr3();
    }
    else
    {
        if( readPDE( virt, pdePtr, addrSpace ) == 0 )
        {
            pdePtr->present = 0;
            returnAddr = ( addr_t )( pdePtr->base << 12 );

            if( writePDE( virt, pdePtr, addrSpace ) == -1 )
            {
              assert(false);
              returnAddr = (addr_t)NULL_PADDR;
            }
        }
        else
        {
          kprintf("unmapPage() : Not present.");
          assert(false);
          returnAddr = (addr_t)NULL_PADDR;
        }
    }
    return returnAddr;
}
