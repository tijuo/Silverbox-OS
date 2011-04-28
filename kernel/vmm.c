#include <kernel/mm.h>
#include <kernel/syscall.h>
#include <kernel/memory.h>
#include <kernel/debug.h>
#include <kernel/thread.h>

int sysMap( TCB *tcb, addr_t virt, addr_t phys, size_t pages, int flags, addr_t addrSpace );
int sysMapPageTable( TCB *tcb, addr_t virt, addr_t phys, int flags, addr_t addrSpace );
int sysGrant( TCB *tcb, addr_t srcAddr, addr_t destAddr,
    addr_t addrSpace, size_t pages );
int sysGrantPageTable( TCB *tcb, addr_t srcAddr, addr_t destAddr,
    addr_t addrSpace, size_t tables );
void *sysUnmap( TCB *tcb, void *virt, addr_t addrSpace );
void *sysUnmapPageTable( TCB *tcb, void *virt, addr_t addrSpace );

int sysMap( TCB *tcb, addr_t virt, addr_t phys, size_t pages, int flags, addr_t addrSpace )
{
  int result, retval=0;

  assert( phys != (addr_t)NULL_PADDR );

  if( phys == (addr_t)NULL_PADDR )
    return -1;

  for( ; pages > 0; pages--, virt += PAGE_SIZE, phys += PAGE_SIZE )
  {
    if( addrSpace == NULL_PADDR )
      result = mapPage( virt, phys, PAGING_RW | PAGING_USER, tcb->addrSpace );
    else
      result = mapPage( virt, phys, PAGING_RW | PAGING_USER, addrSpace );

    assert( result == 0 );

    if( result == 0 )
      retval++;
    else
      return retval;
  }

  return retval;
}

int sysMapPageTable( TCB *tcb, addr_t virt, addr_t phys, int flags, addr_t addrSpace )
{
  int result;

  assert( phys != (addr_t)NULL_PADDR );

  if( phys == (addr_t)NULL_PADDR )
    return -1;

  if( addrSpace == NULL_PADDR )
    result = mapPageTable( virt, phys, PAGING_RW | PAGING_USER, tcb->addrSpace );
  else
    result = mapPageTable( virt, phys, PAGING_RW | PAGING_USER, addrSpace );

  assert( result == 0 );

  return result;
}

int sysGrant( TCB *tcb, addr_t srcAddr, addr_t destAddr, addr_t addrSpace,
    size_t pages )
{
    int result, retval=0;

    addr_t physPage;

    assert( addrSpace != NULL_PADDR );

    if( addrSpace == NULL_PADDR )
      return -1;

    for( ; pages > 0; pages--, srcAddr += PAGE_SIZE, destAddr += PAGE_SIZE )
    {
      physPage = unmapPage( srcAddr, tcb->addrSpace );

      if( physPage == NULL_PADDR )
        return retval;

      result = mapPage( destAddr, physPage, PAGING_RW | PAGING_USER,
               addrSpace );

      assert( result == 0 );

      if( result == 0 )
        retval++;
      else
      {
        // FIXME: Remap the unmapped page
        return retval;
      }
    }

    return retval;
}

int sysGrantPageTable( TCB *tcb, addr_t srcAddr, addr_t destAddr, 
    addr_t addrSpace, size_t tables )
{
    int result, retval=0;

    addr_t physPage;

    assert( addrSpace != NULL_PADDR );

    if( addrSpace == NULL_PADDR )
      return -1;

    for( ; tables > 0; tables--, srcAddr += TABLE_SIZE, destAddr += TABLE_SIZE )
    {
      physPage = unmapPageTable( srcAddr, tcb->addrSpace );

      if( physPage == NULL_PADDR )
        return retval;

      result = mapPageTable( destAddr, physPage, PAGING_RW | PAGING_USER,
               addrSpace );

      assert(result == 0);

      if( result == 0 )
       retval++;
      else
      {
        // FIXME: Remap unmapped page table
        return retval;
      }
    }

    return retval;
}

void *sysUnmap( TCB *tcb, void *virt, addr_t addrSpace )
{
  if( addrSpace == NULL_PADDR )
    return unmapPage( virt, tcb->addrSpace );
  else
    return unmapPage( virt, addrSpace );
}

void *sysUnmapPageTable( TCB *tcb, void *virt, addr_t addrSpace )
{
  if( addrSpace == NULL_PADDR )
    return unmapPageTable( virt, tcb->addrSpace );
  else
    return unmapPageTable( virt, addrSpace );
}
