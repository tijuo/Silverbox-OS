#include "addr_space.h"
#include "phys_alloc.h"
#include <oslib.h>
#include <string.h>

extern int _mapMem( void *phys, void *virt, int pages, int flags, struct AddrSpace *aSpace );
extern void print(const char *, ...);

void dump_regs( struct Tcb *tcb );
int handle_exception( int args[5] );

void dump_regs( struct Tcb *tcb )
{
  print( " @ EIP: 0x" );
  print(toHexString( tcb->state.eip ));

  print( "\nEAX: 0x" );
  print(toHexString( tcb->state.eax  ));
  print( " EBX: 0x" );
  print(toHexString( tcb->state.ebx  ));
  print( " ECX: 0x" );
  print(toHexString( tcb->state.ecx  ));
  print( " EDX: 0x" );
  print(toHexString( tcb->state.edx  ));

  print( "\nESI: 0x" );
  print(toHexString( tcb->state.esi  ));
  print( " EDI: 0x" );
  print(toHexString( tcb->state.edi  ));

  print( " ESP: 0x" );
  print(toHexString( tcb->state.esp  ));
  print( " EBP: 0x" );
  print(toHexString( tcb->state.ebp ));

  print( " CR3: 0x" );
  print(toHexString( (int)tcb->addr_space ) );

  print("\nEFLAGS: 0x");
  print(toHexString( tcb->state.eflags ));
}

int handle_exception( int args[5] )
{
  struct ResourcePool *pool;
  struct Tcb tcb;
  void *addr;
  tid_t tid = args[1];
  int intNum = args[2];
  int errorCode = args[3];
  dword cr2 = args[4];

  pool = lookup_tid(tid);

  sys_read(RES_TCB, &tcb);

  if(intNum == 14)
  {
  /* Only accept if exception was caused by accessing a non-existant user page.
     Then check to make sure that the accessed page was allocated to the thread. */

    if( pool && (errorCode & 0x5) == 0x4 &&
         find_address(&pool->addr_space, (void *)cr2))
    {
      addr = alloc_phys_page(NORMAL, (void *)tcb.addr_space);

      _mapMem( addr, (void *)(cr2 & ~0xFFF), 1, 0, &pool->addr_space );
      tcb.status = TCB_STATUS_READY;
      sys_update(RES_TCB, &tcb);
      return 0;
    }
    else if( pool && (errorCode & 0x05) &&
             (cr2 & ~0x3FFFFF) == STACK_TABLE ) /* XXX: This can be done better. Will not work if there aren't
                                                                                       any pages in the stack page! */
    {
      addr = alloc_phys_page(NORMAL, (void *)tcb.addr_space);

      _mapMem( addr, (void *)(cr2 & ~0xFFF), 1, 0, &pool->addr_space );
      tcb.status = TCB_STATUS_READY;
      sys_update(RES_TCB, &tcb);
      return 0;
    }
    else
    {
      if( !pool )
        print("Invalid pool\n");

      dump_regs( &tcb );
      print("Can't find address: 0x");
      print(toHexString(cr2));
      print(" in addr space 0x");
      print(toHexString(tcb.addr_space));

      return -1;
    }
  }
  else
  {
//    print("Exception!!!(Need to put the rest of data here)\n");
    dump_regs( &tcb );
    return -1;
  }
}
