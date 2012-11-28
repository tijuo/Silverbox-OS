#include <oslib.h>
#include "paging.h"
#include "phys_alloc.h"
#include <string.h>

extern void print( char * );

/* Maps a physical address range to a virtual address range in some address space. */

// XXX: Implement flags

int _mapMem( void *phys, void *virt, int pages, int flags, struct AddrSpace *aSpace )
{
  unsigned char *addr = (unsigned char *)virt;
  void *table_phys;

  if( (unsigned)virt == 0xF0000000u )
  {
    print("virt = 0xF0000000\n");
    printInt(pages);
  }

  if( pages < 1 || !aSpace )
    return -1;

  while(pages--)
  {
    if( get_ptable_status(aSpace, addr) == false )
    {
      table_phys = alloc_phys_page(NORMAL, aSpace->phys_addr);

      clearPage( table_phys );

      __map_page_table( addr, table_phys, 0, aSpace->phys_addr );

      set_ptable_status(aSpace, addr, true);
    }

    // XXX: modify phys page attributes

    __map( addr, phys, 1, 0, aSpace->phys_addr );

    addr = (void *)((unsigned)addr + PAGE_SIZE);
    phys = (void *)((unsigned)phys + PAGE_SIZE);
  }

  return 0;
}

void *_unmapMem( void *virt, struct AddrSpace *aSpace )
{
  // XXX: Need to clear the page table if it is empty!

  return __unmap( virt, aSpace ? aSpace->phys_addr : NULL_PADDR );
}

// Maps a virtual address to a physical address (in this address space)

int mapMemRange( void *virt, int pages )
{
  unsigned i=0;
  void *addr;

  if( pages < 0 )
    return -1;

  while( pages-- )
  {
    addr = alloc_phys_page(NORMAL, page_dir);

    _mapMem( addr, (void *)((unsigned)virt + i++ * PAGE_SIZE), 1, 0,
      &initsrv_pool.addrSpace );
  }

  return 0;
}

void setPage( void *page, char data )
{
  if( __map((void *)TEMP_PAGE, page, 1, 0, NULL_PADDR) < 1 )
  {
    print("Map failed.");
    return;
  }

  memset((void *)TEMP_PAGE, data, PAGE_SIZE);
  __unmap((void *)TEMP_PAGE, NULL_PADDR);

}

void clearPage( void *page )
{
  setPage(page, 0);
}

int peekPage(void *phys, void *data)
{
  if( __map((void *)TEMP_PAGE, phys, 1, 0, NULL_PADDR) < 1 )
    return -1;

  memcpy( data, (void *)TEMP_PAGE, PAGE_SIZE);
  __unmap((void *)TEMP_PAGE, NULL_PADDR);

  return 0;
}

int pokePage(void *phys, void *data)
{
  if( __map((void *)TEMP_PAGE, phys, 1, 0, NULL_PADDR) < 1 )
    return -1;

  memcpy( (void *)TEMP_PAGE, data, PAGE_SIZE);
  __unmap((void *)TEMP_PAGE, NULL_PADDR);

  return 0;
}
