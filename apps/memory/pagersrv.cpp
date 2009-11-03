#include <os/services.h>
#include <os/message.h>
#include <stdlib.h>
#include <elf.h>
#include "AllocTable.h"
#include "PageAlloc.h"
//#include "NameTable.h"

#define MAX_NAME_LENGTH 50

#define PAGE_SIZE 4096

/* There needs to be a way to tell whether the page tables of a page directory are mapped or not.
   This can be done by using a bitmap. Have each bit represent a page table. */


/*               THE PLAN




- Allow the pager to receive exception/page fault/exit messages from the kernel
  by allocating mailbox 1 to a message queue.
- Get an allocPage() service to work.

*/


extern int init_page_lists(void *table_address, unsigned num_pages, void *start_used_addr, unsigned pages_used);

extern "C" void print( char *str );

/*
void print( char *str )
{
  char volatile *vidmem = (char *)0xB8000 + 160 * 15;
  static int i=0;

  while(*str)
  {
    if( *str == '\n' )
    {
      i += 160 - (i % 160);
      str++;
      continue;
    }
    vidmem[i++] = *str++;
    vidmem[i++] = 7; 
  }
}
*/
/* Get all of the bootstrap info, and put it into memory. Then build the allocators */

int get_boot_info( int argc, char **argv )
{
  unsigned int bytes_to_allocate=0;
  unsigned int i, pages_needed=0, start_page_addr, max_mem_addr, max_mem_length;
  char *ptr;

  server_name = argv[0];
  boot_info = (struct BootInfo *)argv[1];
  memory_areas = (struct MemoryArea *)argv[2];
  boot_modules = (struct BootModule *)argv[3];
  page_dir = (unsigned int *)argv[4];

  max_mem_addr = memory_areas[0].base;
  max_mem_length = memory_areas[0].length;

  for(i=0; i < boot_info->num_mem_areas; i++)
  {
    if( memory_areas[i].base > max_mem_addr )
    {
      max_mem_addr = memory_areas[i].base;
      max_mem_length = memory_areas[i].length;
    }
  }

  total_pages = max_mem_addr / PAGE_SIZE + max_mem_length;

  ptr = (char *)allocEnd;

  bytes_to_allocate = total_pages * sizeof(struct PhysPage)/*length * 4 + 3 * sizeof(PageAllocator)*/ + 
     strlen(server_name) + 1 + sizeof(struct BootInfo) + 
     sizeof(struct MemoryArea) * boot_info->num_mem_areas +
     sizeof(struct BootModule) * boot_info->num_mods;

/* Find out how many pages we need for everything. */

  for(i=bytes_to_allocate; i > 0; )
  {
    pages_needed++;

    if( i <= PAGE_SIZE )
      break;

    i -= PAGE_SIZE;
  }

  for(i=0; i < boot_info->num_mem_areas; i++)
  {
    if( memory_areas[i].base >= 0x1000000 && memory_areas[i].length >= pages_needed + 1 )
    {
      start_page_addr = memory_areas[i].base;
      break;
    }
  }

  /* Extra page is needed for page table */

  /* Map in pages for boot information, memory map, etc... */

  __map_page_table(allocEnd, (void *)start_page_addr);
  __map(allocEnd, (void *)(start_page_addr + PAGE_SIZE), pages_needed);
  pages_needed++;

  /* Initialize the physical page lists. */

  init_page_lists(allocEnd, total_pages, allocEnd, pages_needed);

  bytes_to_allocate = total_pages * sizeof(struct PhysPage);/*4 * length + 3 * sizeof(PageAllocator)*/

  /* Copy the server name, boot information, memory map, and modules */

  memcpy(&ptr[bytes_to_allocate], server_name, strlen(server_name) + 1);
  bytes_to_allocate += strlen(server_name) + 1;
  memcpy(&ptr[bytes_to_allocate], boot_info, sizeof *boot_info);
  bytes_to_allocate += sizeof *boot_info;
  memcpy(&ptr[bytes_to_allocate], memory_areas, boot_info->num_mem_areas * sizeof *memory_areas );
  bytes_to_allocate += boot_info->num_mem_areas * sizeof *memory_areas;
  memcpy(&ptr[bytes_to_allocate], boot_modules, boot_info->num_mods * sizeof *boot_modules );
  bytes_to_allocate += boot_info->num_mods * sizeof *boot_modules;

  /* 'start_page_addr' to '(start_page_addr + pages_needed * PAGE_SIZE)' is used physical memory */

  allocEnd = (void *)((unsigned)allocEnd + bytes_to_allocate);
  availBytes = PAGE_SIZE - ((unsigned)allocEnd & (PAGE_SIZE - 1));

/*
  mappingTable.map( 1, (void *)page_dir );
  mappingTable.setPTable( (void *)page_dir, allocEnd ); */

//  allocTable.insert( (memreg_addr_t) 0x1000, 10000, (void *)(0x1032) );
  return 0;
}

/* Possible bug here. This would conflict with the page fault committer.
   allocEnd would be extended past expected pages and the structure will
   point to garbage. */

void mapPage( void )
{
  void * virt, *phys;

  if ( reinterpret_cast<unsigned>( allocEnd ) % 4096 != 0 )
    virt = reinterpret_cast<void*>( ( reinterpret_cast<unsigned>( allocEnd ) & 0xFFFFF000 ) + 0x1000 );
  else
    virt = allocEnd;

  if( !mappingTable.pTableIsMapped( (void *)page_dir, virt ) )
  {
    phys = pageAllocator->alloc();
    __map_page_table( virt, phys );
    mappingTable.setPTable( (void *)page_dir, virt);
  }

  phys = pageAllocator->alloc();

  __map( virt, phys, 1 );
}

extern "C" void mapVirt( void *virt, int pages )
{
  unsigned char *addr = (unsigned char *)virt;
  void *phys;

  if( pages < 1 )
    return;

  while(pages--)
  {
    if( !mappingTable.pTableIsMapped( (void *)page_dir, addr ) )
    {
      phys = pageAllocator->alloc();
      __map_page_table( addr, phys );
      mappingTable.setPTable( (void *)page_dir, addr);
    }

    phys = pageAllocator->alloc();
    __map(addr, phys, 1);
    addr += 4096;      
  }
}


bool isValidElfExe( void *img )
{
  elf_header_t *image = ( elf_header_t * ) img;

  if( img == NULL )
    return false;

  if ( memcmp( "\x7F""ELF", image->identifier, 4 ) != 0 )
    return false;

  else
  {
    if ( image->identifier[ EI_VERSION ] != EV_CURRENT )
      return false;
    if ( image->type != ET_EXEC )
      return false;
    if ( image->machine != EM_386 )
      return false;
    if ( image->version != EV_CURRENT )
      return false;
    if ( image->identifier[ EI_CLASS ] != ELFCLASS32 )
      return false;
  }

  return true;

}

// !!! Requires physical pages for BSS !!! 

// Note that img is a physical address and needs to be mapped to an address somewhere

int load_elf_exec( struct BootModule *module )//, mid_t exHandler, addr_t addrSpace, addr_t stack, addr_t uStack )
{
  unsigned phtab_count, i, j;
  elf_header_t *image = (elf_header_t *)0xC0000000;//module->start_addr;
  elf_pheader_t *pheader;
  tid_t tid;
  void *phys;
  size_t length = module->mod_end - module->mod_start;
  void *addrSpace, *temp;
  void *tempPage;
  unsigned lastTable = 1;

  tempPage = pageAllocator->alloc();

  __map_page_table((void *)image, tempPage);
  __map((void *)image, (void *)module->mod_start, (length % PAGE_SIZE == 0 ? (length / PAGE_SIZE) : (length / PAGE_SIZE) + 1));

  if( !isValidElfExe( image ) )
  {
    print("Not a valid ELF executable.\n");
    return -1;
  }

  phtab_count = image->phnum;

  addrSpace = pageAllocator->alloc();

  __map( (void *)0x10000, addrSpace, 1 );
  memset( (void *)0x10000, 0, PAGE_SIZE );
  __unmap( (void *)0x10000 );

  temp = pageAllocator->alloc();
/*
  __map( (void *)0x10000, temp, 1 );
  memset( (void *)0x10000, 0, PAGE_SIZE );
  __unmap( (void *)0x10000 );
*/
  __map_page_table( (void *)0xE0000000, temp);
  __map((void *)0xE03FF000, pageAllocator->alloc(), 1 );

  tid = __create_thread( (addr_t)image->entry, addrSpace, (void *)0xE0400000, 1 );

  if( tid == -1 )
    return -1;

  mappingTable.map( tid, addrSpace );

  __grant_page_table( (void *)0xE0000000, (void *)0xE0000000, addrSpace, 1 );
       
  // Program header information is loaded into memory

  pheader = (elf_pheader_t *)((unsigned)image + image->phoff);

  for ( i = 0; i < phtab_count; i++, pheader++ )
  {
    if ( pheader->type == PT_LOAD )
    {
      unsigned memSize = pheader->memsz;
      unsigned fileSize = pheader->filesz;

      for ( j = 0; memSize > 0; j++ )
      {
        if ( fileSize == 0 )
          // Picks a physical address for BSS
          phys = pageAllocator->alloc();
        else
          phys = (void *)(pheader->offset + (unsigned)module->mod_start + j * PAGE_SIZE);

        if( !mappingTable.pTableIsMapped( addrSpace, (void *)((unsigned)pheader->vaddr + j * PAGE_SIZE) ) )
        {
          if( (lastTable != ((unsigned)pheader->vaddr + j * PAGE_SIZE) & 0xFFC00000 ) && (lastTable != 1) )
            __grant_page_table((void *)0x4000000, (void *)lastTable, addrSpace, 1);

          /* For some strange reason, the end of memory is filled with 0xFFFFFFF and can't be written to.
             Maybe it's memory mapped I/O? */

          temp = (void *)pageAllocator->alloc();
/*
          __map( (void *)0x10000, temp, 1 );
          memset( (void *)0x10000, 0, PAGE_SIZE );
          __unmap( (void *)0x10000 );
*/
          __map_page_table((void *)0x4000000, temp);

          lastTable = ((unsigned)pheader->vaddr + j * PAGE_SIZE) & 0xFFC00000;

          mappingTable.setPTable( addrSpace, (void *)((unsigned)pheader->vaddr + j * PAGE_SIZE) );
        }

        __map( (void *)(0x4000000 + (((unsigned)pheader->vaddr + j * PAGE_SIZE) & 0x3FFFFF)), phys, 1 );

        if( fileSize == 0 )
          memset( (void *)(0x4000000 + (((unsigned)pheader->vaddr + j * PAGE_SIZE) & 0x3FFFFF)), 0, 4096 );

        if( memSize < PAGE_SIZE )
          memSize = 0;
        else
          memSize -= PAGE_SIZE;

        if( fileSize < PAGE_SIZE )
          fileSize = 0;
        else
          fileSize -= PAGE_SIZE;
      }
    }
  }

  if( lastTable != 1 )
    __grant_page_table((void *)0x4000000, (void *)lastTable, addrSpace, 1);

  __start_thread( tid );

  for(i=0; i < (length % PAGE_SIZE == 0 ? (length / PAGE_SIZE) : (length / PAGE_SIZE) + 1); i++)
    __unmap((void *)((unsigned)image + i * PAGE_SIZE));
/*
  __unmap_page_table( image );
  pageAllocator->free( tempPage );
*/
  return 0;
}


/* Handles exceptions, memory allocations, i/o permissions,
   DMA, address spaces, exit messages */
/*
struct MessageQueue servQueue;
struct Message mboxQueue[32];
*/

//unsigned int pointer=0, tail=0;

unsigned int pointer = 0, tail = 0;
struct Message mboxQueue[8];
struct MessageQueue servQueue;

void initMsgQueue( void )
{
  servQueue.length = 8 * sizeof(struct Message);
  servQueue.queueAddr = mboxQueue;
  servQueue.pointer = &pointer;
  servQueue.tail = &tail;
}

int main( int argc, char **argv )
{
  //unsigned char buffer[ 256 ];
  //struct MessageHeader header, replyHeader;
 // int result;

  struct Message msg;
  struct MessageHeader *header = (struct MessageHeader *)msg.data;
  mid_t mbox;

  initMsgQueue();

  allocEnd = (void *)0x1000000;
  availBytes = 0;
  get_boot_info(argc, argv);
//  __map((void *)0xB8000, (void *)0xB8000, 8);

  /* !! Figure out how to get argc and argv from kernel !! */

  mbox = __alloc_mbox(1, &servQueue);

  if( mbox < 0 )
  {
    print("Couldn't allocate mbox 1\n");
     __exit(-1);
  }

  load_elf_exec(&boot_modules[1]);

  while ( true )
  {
    __test_for_msg(1, true);
    __get_message(&servQueue, &msg);

    switch ( header->subject )
    {
      case SYS_EXCEPTION_MSG:
      {
        void *physAddr;
        print("pager: Page fault received.\n");

        struct ExceptionInfo *info = reinterpret_cast<ExceptionInfo *>( header+1 );

        if( info->state.intNum == 14)
        {
        /* Only accept if exception was caused by accessing a non-existant user page.
           Then check to make sure that the accessed page was allocated to the thread. */

          if ( (info->state.errorCode & 0x5) == 0x4 && allocTable.find( (memreg_addr_t)info->cr2, (void *)info->cr3) )
          {
            physAddr = pageAllocator->alloc();

            __map( (void *)0x10000, (void *)physAddr, 1 );

            __grant( (void *)0x10000, (void *)info->cr2, (void *)info->cr3, 1 );
            __end_page_fault(info->tid);
          }
        }
      }
      break;
/*
      case ALLOC_MEM:
      {
        struct AllocMemInfo *info = reinterpret_cast<AllocMemInfo *>( header+1 );
        struct AllocMemReply replyMsg;
        void *addr = (void *)info->addr;
        unsigned numPages = (unsigned)info->numPages;
        tid_t sender = (pid_t)info->sender;

        result = allocTable.insert( ( void * ) ( ( unsigned )info->addr & 0xFFFFF000 ), info->numPages * 0x1000, info->sender );

        if( header->reply > 0 )
        {
          replyMsg.subject = ALLOC_MEM;
          replyMsg.reply = -1;
          replyMsg.length = sizeof( u32 );
          *(int *)&replyMsg.data = 0;

          __post_message(__outqueue, &replyMsg);
        }
      }
      break;

      case EXIT_MSG:
      {
        mappingTable.unmap(tid, addr);
      }
      break;

      case MAP_TID:
      {
        tid_t tid;
        unsigned char *addr;

        mappingTable.map(tid, addr);
      }
      break;
*/
      default:
        print( "Invalid message: 0x" );
        //printNum( header.subject, true );
     //                     send( replyMbox, header.subject, NULL, 0 );
        break;
    }
  }

  __release_mbox( mbox );

  return 0;
}

