#include <kernel/mm.h>
#include <oslib.h>
#include <kernel/debug.h>
#include <kernel/thread.h>
#include <kernel/memory.h>

//static void mapPageTable( pde_t *pde, void *phys, u32 privilege );
//static bool tempMapped = false, tempMapped2 = false;

// XXX: Are these functions correct?

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
int mapPageTable( void *phys, void *virt, u32 privilege, 
     void *addrSpace );
int mapPage( void *phys, void *virt, u32 privilege, void *addrSpace );
addr_t unmapPageTable( void *virt, void *addrSpace );
addr_t unmapPage( void *virt, void *addrSpace );

void reload_cr3(void);

// XXX: mapPage()
// XXX: unmapPage()

/* when I added mapTemp() and unmapTemp(), weird things started to happen. */

void reload_cr3(void)
{
  __asm__ volatile("push %eax\n"
                   "movl %cr3, %eax\n"
                   "movl %eax, %cr3\n"
                   "pop %eax\n" );
}
/*
int mapTemp( void *phys )
{
  int val;
  
  assert( phys != (void *)NULL_PADDR );
  assert( !tempMapped );
  assert( (u32)phys == ((u32)phys & ~0xFFF) );

  pdePtr = ADDR_TO_PDE( virt );
  ptePtr = ADDR_TO_PTE( virt );

  if ( !pdePtr->present )
  {
    assert( false );
    return -2;
  }

  *(u32 *)ptePtr = (u32)phys | privilege | 1;

  __asm__ volatile( "invlpg %0\n" :: "m"( *(char *)virt ) );

//  if( !tempMapped )
//  {
    val = _mapTemp(phys);
    assert(val == 0);

    if( val == 0 )
      tempMapped = true;
    return val;
//  }
 // else
 //   return -1;
}

void unmapTemp( void )
{
  unsigned val;

  assert( tempMapped );

  if( tempMapped )
  {
    val = (unsigned)_unmapTemp();
    assert(val != (unsigned)NULL_PADDR);

    if( val != (unsigned)NULL_PADDR )
      tempMapped = false;
  }
}
*/
/* !!! Warning: When modifying a page directory entry in kernel space, all page
       directories need to be accessible and modified. !!! */

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

int writePDE( void *virt, pde_t *pde, void *addrSpace )
{
  assert( virt != (void *)INVALID_VADDR );
  assert( addrSpace != (void *)NULL_PADDR );

  if( virt == (void *)INVALID_VADDR || addrSpace == NULL_PADDR )
    return -1;

  return _writePDE( (unsigned)virt >> 22, pde, addrSpace );
}

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
    kprintf("Virt: 0x%x PTE: 0x%x AddrSpace: 0x%x\n", virt, pte, 
addrSpace/*,  *(unsigned *)&pde*/);
    assert(false);
    return -1;
  }

  if( (unsigned)addrSpace == (unsigned)getCR3() )
  {
    ptePtr = (pte_t *)ADDR_TO_PTE( virt );
    *ptePtr = *pte;
    __asm__ volatile( "invlpg %0\n" :: "m"( *(char *)virt ) );
  }
  else
  {
    mapTemp( (addr_t)(pde.base << 12) );
    table->pages[tableEntryNum] = *pte;
    unmapTemp();
  }
  return 0;
}

/* XXX: This is *really* broken for some reason... */

/* address is the foreign address(in the indicated addrSpace */
/* buffer is current address space */

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

/* data in 'buffer' goes to 'address' */

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

int mapPageTable( void *virt, void *phys, u32 privilege, void *addrSpace )
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
    *(u32 *)pdePtr = (u32)phys | privilege | 1;
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
    return -1;
  }
  return 0;
}

int kMapPage( void *virt, void *phys, u32 privilege)
{
  pde_t *pdePtr;
  pte_t *ptePtr;

  assert( virt != (void *)INVALID_VADDR );
  assert( phys != (void *)NULL_PADDR );
/*
  if( virt == (void *)INVALID_VADDR || 
      phys == (void *)NULL_PADDR )
  {
    kprintf("mapPage(): Error!!! ");
    return -1;
  }
*/
  if( (u32)phys != ((u32)phys & ~0xFFF))
    kprintf("phys: 0x%x\n", phys);

  assert( (u32)phys == ((u32)phys & ~0xFFF) );

  pdePtr = ADDR_TO_PDE( virt );
  ptePtr = ADDR_TO_PTE( virt );

  if ( !pdePtr->present )
  {
    kprintf("%x -> %x\n", virt, phys);
    assert( false );
    return -1;
  }

  *(u32 *)ptePtr = (u32)phys | privilege | 1;

  __asm__ volatile( "invlpg %0\n" :: "m"( *(char *)virt ) );

  return 0;
}

/* This is supposed to map a virtual address to a physical address in any
   address space. */

int mapPage( void *virt, void *phys, u32 privilege, void *addrSpace )
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
    return -1;
  }

  if( currPdir != addrSpace )
  {
    if( readPTE( virt, ptePtr, addrSpace ) < 0 )
    {
      assert(false);
      return -1;
    }
  }

  *(u32 *)ptePtr = (u32)phys | privilege | 1;

  if ( currPdir == addrSpace )
  {
    __asm__ volatile( "invlpg %0\n" :: "m"( *(char *)virt ) );
    assert( *(u32 *)ptePtr & 0x01 );
  }
  else
    writePTE( virt, ptePtr, addrSpace );

  return 0;
}

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
    __asm__ volatile( "invlpg %0\n" :: "m" ( *(char *)virt ) );

    return (addr_t)returnAddr;
  }
  else
    return (addr_t)NULL_PADDR;
}

/* XXX: This needs work */

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

	  __asm__ volatile( "invlpg %0\n" :: "m" ( *(char *)virt ) );
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
