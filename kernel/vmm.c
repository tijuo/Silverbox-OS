#include <kernel/mm.h>
#include <kernel/syscall.h>
#include <kernel/memory.h>
#include <kernel/debug.h>
#include <kernel/thread.h>

// TODO: map, grant, and unmap operations should be privileged syscalls
// TODO: only the inital server should have access to them

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
  int result;

  assert( tcb != NULL );
  assert( phys != (addr_t)NULL_PADDR );

  if( phys == (addr_t)NULL_PADDR || tcb == NULL )
    return -1;

  for( ; pages > 0; pages--, virt += PAGE_SIZE, phys += PAGE_SIZE )
  {
    #ifdef DEBUG2
    disableInt();
    #endif

    if( addrSpace == NULL_PADDR )
      result = mapPage( virt, phys, PAGING_RW | PAGING_USER, tcb->addrSpace );
    else
      result = mapPage( virt, phys, PAGING_RW | PAGING_USER, addrSpace );

    #ifdef DEBUG2
    enableInt();
    #endif

    assert( result == 0 );
  }
  return 0;
}

int sysMapPageTable( TCB *tcb, addr_t virt, addr_t phys, int flags, addr_t addrSpace )
{
  int result;

  assert( tcb != NULL );
  assert( phys != (addr_t)NULL_PADDR );

  if( phys == (addr_t)NULL_PADDR || tcb == NULL )
    return -1;

  #ifdef DEBUG2
  disableInt();
  #endif

  if( addrSpace == NULL_PADDR )
    result = mapPageTable( virt, phys, PAGING_RW | PAGING_USER, tcb->addrSpace );
  else
    result = mapPageTable( virt, phys, PAGING_RW | PAGING_USER, addrSpace );

  #ifdef DEBUG2
  enableInt()
  #endif

  assert( result == 0 );

  return result;
}

int sysGrant( TCB *tcb, addr_t srcAddr, addr_t destAddr, addr_t addrSpace, 
    size_t pages )
{
    #ifdef DEBUG
    int result;
    #endif /* DEBUG */

    addr_t physPage;

    assert( tcb != NULL );
    assert( addrSpace != NULL_PADDR );

    if( tcb == NULL || addrSpace == NULL_PADDR )
      return -1;

    for( ; pages > 0; pages--, srcAddr += PAGE_SIZE, destAddr += PAGE_SIZE )
    {
      physPage = unmapPage( srcAddr /*+ (i * PAGE_SIZE)*/, tcb->addrSpace );

      #ifdef DEBUG
      result =
      #endif /* DEBUG */

        mapPage( destAddr /*+  (i * PAGE_SIZE)*/, physPage, PAGING_RW | PAGING_USER,
               addrSpace );

      assert( result == 0 );
    }

    return 0;
}

int sysGrantPageTable( TCB *tcb, addr_t srcAddr, addr_t destAddr, 
    addr_t addrSpace, size_t tables )
{
    #ifdef DEBUG
    int result;
    #endif /* DEBUG */

    addr_t physPage;

    assert( tcb != NULL );
    assert( addrSpace != NULL_PADDR );

    if( tcb == NULL || addrSpace == NULL_PADDR )
      return -1;

    for( ; tables > 0; tables--, srcAddr += TABLE_SIZE, destAddr += TABLE_SIZE )
    {
      physPage = unmapPageTable( srcAddr, tcb->addrSpace );

//      kprintHex((int)destAddr);
//      kprint(" ");
      #ifdef DEBUG
      result =
      #endif /* DEBUG */

      mapPageTable( destAddr, physPage, PAGING_RW | PAGING_USER,
               addrSpace );

      assert(result == 0);
    }

    return 0;
}

void *sysUnmap( TCB *tcb, void *virt, addr_t addrSpace )
{
  assert( tcb != NULL );

  if( addrSpace == NULL_PADDR )
    return unmapPage( virt, tcb->addrSpace );
  else
    return unmapPage( virt, addrSpace );
}

void *sysUnmapPageTable( TCB *tcb, void *virt, addr_t addrSpace )
{
  assert( tcb != NULL );

  if( addrSpace == NULL_PADDR )
    return unmapPageTable( virt, tcb->addrSpace );
  else
    return unmapPageTable( virt, addrSpace );
}

