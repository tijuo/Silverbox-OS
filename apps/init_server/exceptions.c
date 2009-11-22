#include "addr_space.h"
#include "phys_alloc.h"
#include <oslib.h>
#include <string.h>

#define STACK_TABLE	0xE0000000
#define TEMP_PTABLE	0xE0000000
#define TEMP_PAGE	0x10000
#define KPHYS_START	0x100000

extern int _mapMem( void *phys, void *virt, int pages, int flags, void *pdir );

void dump_regs( int cr2, struct ThreadInfo *info );
void handle_exception( tid_t tid, int cr2 );

void dump_regs( int cr2, struct ThreadInfo *info )
{
  print( "\n\n\n\n\nException: " );
  print(toIntString( info->state.int_num ) );
  print( " @ EIP: 0x" );
  print(toHexString( info->state.eip ));

  print( " TID: ");
  print( toIntString(info->tid) );

  print( "\nEAX: 0x" );
  print(toHexString( info->state.eax  ));
  print( " EBX: 0x" );
  print(toHexString( info->state.ebx  ));
  print( " ECX: 0x" );
  print(toHexString( info->state.ecx  ));
  print( " EDX: 0x" );
  print(toHexString( info->state.edx  ));

  print( "\nESI: 0x" );
  print(toHexString( info->state.esi  ));
  print( " EDI: 0x" );
  print(toHexString( info->state.edi  ));

  print( " ESP: 0x" );
  print(toHexString( info->state.esp  ));
  print( " EBP: 0x" );
  print(toHexString( info->state.ebp ));

  print( "\nDS: 0x" );
  print(toHexString( info->state.ds ));
  print( " ES: 0x" );
  print(toHexString( info->state.es ));

  if( info->state.int_num == 14 )
  {
    print(" CR2: 0x");
    print(toHexString( cr2 ));
  }

  print( " error code: 0x" );
  print(toHexString( info->state.error_code ));

  print( " CR3: 0x" );
  print(toHexString( info->addr_space ) );

  print("\nEFLAGS: 0x");
  print(toHexString( info->state.eflags ));

  print(" User SS: 0x");
  print(toHexString(info->state.userEsp));

  print(" SS: 0x");
  print(toHexString(info->state.userSs));
}

void handle_exception( tid_t tid, int cr2 )
{
  struct ThreadInfo thread_info;

  __get_thread_info( tid, &thread_info );

  if( thread_info.state.int_num == 14 )
  {
  /* Only accept if exception was caused by accessing a non-existant user page.
     Then check to make sure that the accessed page was allocated to the thread. */

    if ( (thread_info.state.error_code & 0x5) == 0x4 && find_address((void *)thread_info.addr_space, 
          (void *)cr2))
    {
      _mapMem( alloc_phys_page(NORMAL, (void *)thread_info.addr_space), 
               (void *)(cr2 & ~0xFFF), 1, 0, (void *)thread_info.addr_space );
      __end_page_fault(thread_info.tid);
    }
    else if( (thread_info.state.error_code & 0x05) && (cr2 & ~0x3FFFFF) == STACK_TABLE ) /* XXX: This can be done better. Will not work if there aren't
                                                                                       any pages in the stack page! */
    {
      _mapMem( alloc_phys_page(NORMAL, (void *)thread_info.addr_space), 
               (void *)(cr2 & ~0xFFF), 1, 0, (void *)thread_info.addr_space );
      __end_page_fault(thread_info.tid);
    }
    else
    {
      dump_regs( cr2, &thread_info );
     // print("Can't find address: 0x"), print(toHexString(info->cr2)), print(" in addr space 0x"), print(toHexString(info->cr3));
    }
  }
  else
  {
//    print("Exception!!!(Need to put the rest of data here)\n");
    dump_regs( cr2, &thread_info );
  }
}
