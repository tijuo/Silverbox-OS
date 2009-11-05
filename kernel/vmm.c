#include <kernel/mm.h>
#include <kernel/syscall.h>
#include <kernel/memory.h>
#include <kernel/debug.h>
#include <kernel/thread.h>

// TODO: map, grant, and unmap operations should be privileged syscalls
// TODO: only the inital server should have access to them

int sysMap( TCB *tcb, addr_t virt, addr_t phys, size_t pages );
int sysMapPageTable( TCB *tcb, addr_t virt, addr_t phys );
int sysGrant( TCB *tcb, addr_t srcAddr, addr_t destAddr, 
    addr_t addrSpace, size_t pages );
int sysGrantPageTable( TCB *tcb, addr_t srcAddr, addr_t destAddr,
    addr_t addrSpace, size_t tables );
void *sysUnmap( TCB *tcb, void *virt );
void *sysUnmapPageTable( TCB *tcb, void *virt );

void *sysUnmap( TCB *tcb, void *virt )
{
  assert( tcb != NULL );
  
  return unmapPage( virt, tcb->addrSpace );
}

void *sysUnmapPageTable( TCB *tcb, void *virt )
{
  assert( tcb != NULL );

  return unmapPageTable( virt, tcb->addrSpace );
}

int sysMap( TCB *tcb, addr_t virt, addr_t phys, size_t pages )
{
  #ifdef DEBUG
  int result;
  #endif

  assert( tcb != NULL );
  assert( phys != (addr_t)NULL_PADDR );

  if( phys == (addr_t)NULL_PADDR || tcb == NULL )
    return -1;

  for( ; pages > 0; pages--, virt += PAGE_SIZE, phys += PAGE_SIZE )
  {
    #ifdef DEBUG2
    disableInt();
    #endif

    #ifdef DEBUG
    result =
    #endif

    mapPage( virt, phys, PAGING_RW | PAGING_USER, tcb->addrSpace );

    #ifdef DEBUG2
    enableInt();
    #endif

    assert( result == 0 );
  }
  return 0;
}

int sysMapPageTable( TCB *tcb, addr_t virt, addr_t phys )
{
  #ifdef DEBUG
  int result;
  #endif

  assert( tcb != NULL );
  assert( phys != (addr_t)NULL_PADDR );

  if( phys == (addr_t)NULL_PADDR || tcb == NULL )
    return -1;

  #ifdef DEBUG2
  disableInt();
  #endif

  #ifdef DEBUG
  result =
  #endif

  mapPageTable( virt, phys, PAGING_RW | PAGING_USER, tcb->addrSpace );

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

