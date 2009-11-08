#include <os/signal.h>
#include <oslib.h>
#include <elf.h>
#include <string.h>
#include <stdlib.h>
#include <os/services.h>
#include <os/uthreads.h>
#include <os/message.h>
#include <os/dev_interface.h>
#include <os/device.h>
#include "shmem.h"

extern void handleTMPO(struct Message *msg);

/* Malloc works here as long as 'pager_addr_space' exists and is correct*/

#define STACK_TABLE	0xE0000000
#define TEMP_PTABLE	0xE0000000
#define TEMP_PAGE	0x10000
#define KPHYS_START	0x100000

struct AddrSpace pager_addr_space;

extern int register_name(char *name, size_t len, tid_t tid);
extern tid_t lookup_name( char *name, size_t len);

extern void print( char * );
extern void *list_malloc( size_t );
extern void list_free( void * );

struct ProgramArgs
{
  char *data;
  int length;
};

extern struct ListType shmem_list;

void handle_message(void);
/*
struct State state1, state2;

void tested(void)
{
  while(1)
  {
    print("a");
    _switch_context(&state1, &state2);
  }
}

void tested2(void)
{
  while(1)
  {
    print("b");
    _switch_context(&state2, &state1);
  }
}
*/

struct NameEntry
{
  char name[N_MAX_NAME_LEN];
  size_t name_len;

  union Entry entry;
  enum NameType name_type;
} names[N_MAX_NAMES];

static unsigned num_names=0;
static unsigned num_fs=0;

static int _register( tid_t tid, enum NameType name_type, union Entry *device );
static int registerDevName( tid_t tid, char *name, size_t name_len, 
                            enum NameType name_type, union Entry *device );
static struct Device *_lookupMajor( unsigned char major );
static struct NameEntry *_lookupName( char *name, size_t len );

static int registerDevName( tid_t tid, char *name, size_t name_len, 
                         enum NameType name_type, union Entry *entry )
{
  if( name == NULL || name_len > N_MAX_NAME_LEN || entry == NULL )
    return -1;

  if( num_names >= N_MAX_NAMES )
    return -1;
  // XXX: Try to find a duplicate name here

  strncpy( names[num_names].name, name, name_len );
  names[num_names].name_len = name_len;

  names[num_names].entry = *entry;
  num_names++;

  return _register(tid, name_type, entry);
}

static int _register( tid_t tid, enum NameType name_type, union Entry *entry )
{
  if( name_type == DEV_NAME )
  {
    if( entry->device.numDevices == 0 || entry->device.dataBlkLen == 0 || 
        (unsigned char)entry->device.major >= MAX_DEVICES )
    {
      return -1;
    }

    if( entry->device.type != CHAR_DEV && entry->device.type != BLOCK_DEV )
      return -1;

    if( devices[entry->device.major].used )
      return 1;

    devices[entry->device.major] = entry->device;
    devices[entry->device.major].used = 1;
    devices[entry->device.major].ownerTID = tid;

    return 0;
  }
  else
  {
    if( num_fs >= MAX_FILESYSTEMS )
      return -1;

    filesystems[num_fs] = entry->fs;
    filesystems[num_fs++].ownerTID = tid;

    return 0;
  }
}

static struct Device *_lookupMajor( unsigned char major )
{
  if( (unsigned char)major >= MAX_DEVICES )
    return NULL;

  if( !devices[major].used )
    return NULL;

  return &devices[major];
}

static struct NameEntry *_lookupName( char *name, size_t len )
{
  if( name == NULL || len >= N_MAX_NAME_LEN )
    return NULL;

  for(unsigned i=0; i < N_MAX_NAMES; i++)
  {
    if( strncmp(names[i].name, name, len) == 0 )
      return &names[i];
  }

  return NULL;
}

int get_boot_info( int argc, char **argv )
{
  unsigned int bytes_to_allocate=0;
  unsigned int i, pages_needed=0, start_page_addr, max_mem_addr, max_mem_length;
  unsigned temp;
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
    if( memory_areas[i].base >= KPHYS_START && memory_areas[i].length >= pages_needed + 1 )
    {
      start_page_addr = memory_areas[i].base;
      break;
    }
  }

  __map((void *)TEMP_PAGE, (void *)start_page_addr, 1);
  memset((void *)TEMP_PAGE, 0, PAGE_SIZE);
  __unmap((void *)TEMP_PAGE);

  /* Extra page is needed for page table */

  /* Map in pages for boot information, memory map, etc... */

  __map_page_table(allocEnd, (void *)start_page_addr);

  for(i=1; i < pages_needed+1; i++)
  {
    __map((void *)TEMP_PAGE, (void *)(start_page_addr + i * PAGE_SIZE), 1);
    memset((void *)TEMP_PAGE, 0, PAGE_SIZE);
    __unmap((void *)TEMP_PAGE);
  }

  __map(allocEnd, (void *)(start_page_addr + PAGE_SIZE), pages_needed);
  pages_needed += 2; //pages_needed++; <--- FIXME: Why is it only incremented by 1 instead of 2 ???

  /* Initialize the physical page lists. */

  init_page_lists(allocEnd, total_pages, (void *)start_page_addr, pages_needed);
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

  temp = (unsigned)allocEnd;
  allocEnd = (void *)((unsigned)allocEnd + bytes_to_allocate);
  availBytes = PAGE_SIZE - ((unsigned)allocEnd & (PAGE_SIZE - 1));

  init_addr_space(&pager_addr_space, page_dir);
  list_init(&addr_space_list, list_malloc, list_free);
  list_insert((int)page_dir, &pager_addr_space, &addr_space_list);

  attach_tid(page_dir, 1);

  for(i=0; i < bytes_to_allocate; i += PTABLE_SIZE)
    set_ptable_status(page_dir, (void *)(temp + i), true);

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
  void *virt;

  if ( (unsigned)allocEnd % PAGE_SIZE != 0 )
    virt = (void *)( ( (unsigned)allocEnd & 0xFFFFF000 ) + PAGE_SIZE  );
  else
    virt = allocEnd;

  mapVirt( virt, 1 );
}

void _mapMem( void *phys, void *virt, int pages, int flags, void *pdir )
{
  unsigned char *addr = (unsigned char *)virt;
  void *table_phys;
//  if( pdir == 

  if( pages < 1 )
    return;

  while(pages--)
  {
    if( get_ptable_status((void *)pdir, addr) == false )
    {
      table_phys = alloc_phys_page(NORMAL, pdir);

      __map( (void *)TEMP_PAGE, table_phys, 1 );
      memset((void *)TEMP_PAGE, 0, 4096);
      __unmap( (void *)TEMP_PAGE );

      if( pdir == page_dir )
        __map_page_table( addr, table_phys );
      else
      {
        __map_page_table( (void *)TEMP_PTABLE, table_phys );
        __grant_page_table((void *)TEMP_PTABLE, addr, pdir, 1);
      }
      set_ptable_status(pdir, addr, true);
    }

//    phys = alloc_phys_page(NORMAL, pdir);

    // XXX: modify phys page attributes

    if( pdir == page_dir )
      __map( addr, phys, 1 );
    else
    {
      __map( (void *)TEMP_PAGE, phys, 1 );
      __grant( (void *)TEMP_PAGE, addr, pdir, 1 );
    }

    addr += PAGE_SIZE;
    phys += PAGE_SIZE;
  }
}

void mapVirt( void *virt, int pages )
{
  _mapMem( alloc_phys_page(NORMAL, page_dir), virt, pages, 0, page_dir );
}

bool isValidElfExe( void *img )
{
  elf_header_t *image = ( elf_header_t * ) img;

  if( img == NULL )
    return false;

  if( !VALID_ELF(image) )
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

int load_elf_exec( struct BootModule *module, struct ProgramArgs *args )//, mid_t exHandler, addr_t addrSpace, addr_t stack, addr_t uStack )
{
  unsigned phtab_count, i, j;
  elf_header_t *image = (elf_header_t *)0xC0000000;//module->start_addr;
  elf_pheader_t *pheader;
  tid_t tid;
  void *phys;
  size_t length = module->mod_end - module->mod_start;
  void *addrSpace, *temp;;
  void *tempPage;
  unsigned lastTable = 1;
  struct AddrSpace *addr_space_struct;
  unsigned arg_len=8;

  if( module == NULL )
    return -1;

  tempPage = alloc_phys_page(NORMAL, page_dir);//pageAllocator->alloc();

  __map( (void *)TEMP_PAGE, tempPage, 1 );
  memset( (void *)TEMP_PAGE, 0, PAGE_SIZE );
  __unmap( (void *)TEMP_PAGE );

  if( __map_page_table((void *)image, tempPage) != 0 )
  {
    free_phys_page(tempPage);
    print("__map_page_table() failed.\n");
   __exit(1);
  }

  if( __map((void *)image, (void *)module->mod_start, (length % PAGE_SIZE == 0 ? (length / PAGE_SIZE) : (length / PAGE_SIZE) + 1)) != 0)
  {
    __unmap_page_table((void *)image);
    free_phys_page(tempPage);
    print("__map failed!\n");
   __exit(1);
  }

   //__yield(); // This makes the code work for some reason...

  if( !isValidElfExe( image ) )
  {
    __unmap((void *)image);
    __unmap_page_table((void *)image);
    free_phys_page(tempPage);
    print("Not a valid ELF executable.\n");
    return -1;
  }

  phtab_count = image->phnum;

  addrSpace = alloc_phys_page(NORMAL, page_dir);//pageAllocator->alloc();

  // XXX: Now create a 'struct AddrSpace' for the new thread

  addr_space_struct = malloc( sizeof( struct AddrSpace ) );
  init_addr_space(addr_space_struct, addrSpace);
  list_insert((int)addrSpace, addr_space_struct, &addr_space_list);

  __map( (void *)TEMP_PAGE, addrSpace, 1 );
  memset( (void *)TEMP_PAGE, 0, PAGE_SIZE );
  __unmap( (void *)TEMP_PAGE );

  temp = alloc_phys_page(NORMAL, addrSpace);//pageAllocator->alloc();

  __map( (void *)TEMP_PAGE, temp, 1 );
  memset( (void *)TEMP_PAGE, 0, PAGE_SIZE );
  __unmap( (void *)TEMP_PAGE );

  if( __map_page_table( (void *)TEMP_PTABLE, temp) != 0 )
    ;//cleanup;
 
  phys = alloc_phys_page(NORMAL, addrSpace);

  if( __map((void *)(TEMP_PTABLE + PTABLE_SIZE - PAGE_SIZE), phys, 1 ) != 0 )
    ;// cleanup;

  if( args != NULL )
  {
    /* XXX: This may not work */
    memcpy( (void *)(TEMP_PTABLE + PTABLE_SIZE - args->length - 8), args->data, args->length);
    arg_len += args->length;
  }

  memset( (void *)(TEMP_PTABLE + PTABLE_SIZE - 8), 0, 8);

  if( args != NULL )
  {
    ; // Here, put the start argument pointer and return address (if __exit() isn't called)
  }

  tid = __create_thread( (addr_t)image->entry, addrSpace, (void *)(TEMP_PTABLE + PTABLE_SIZE - arg_len), 1 );

  if( tid == -1 )
    return -1; // XXX: But first, free physical memory before returning

  attach_tid(addrSpace, tid); //mappingTable.map( tid, addrSpace );

  __grant_page_table( (void *)TEMP_PTABLE, (void *)STACK_TABLE, addrSpace, 1 );
  set_ptable_status( addrSpace, (void *)STACK_TABLE, true );

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
        {
  //        print("fileSize == 0:\n");
          // Picks a physical address for BSS
          phys = alloc_phys_page(NORMAL, addrSpace); //pageAllocator->alloc();

          __map( (void *)TEMP_PAGE, phys, 1 );
          memset( (void *)TEMP_PAGE, 0, PAGE_SIZE );
          __unmap( (void *)TEMP_PAGE );
        }
        else
          phys = (void *)(pheader->offset + (unsigned)module->mod_start + j * PAGE_SIZE);

        if( get_ptable_status(addrSpace, (void *)((unsigned)pheader->vaddr + j * PAGE_SIZE)) == false )
        {
          if( (lastTable != (((unsigned)pheader->vaddr + j * PAGE_SIZE) & 0xFFC00000) ) && (lastTable != 1) )
            __grant_page_table((void *)0x4000000, (void *)lastTable, addrSpace, 1);

          /* For some strange reason, the end of memory is filled with 0xFFFFFFF and can't be written to.
             Maybe it's for memory mapped I/O? */

          temp = alloc_phys_page(NORMAL, addrSpace);//(void *)pageAllocator->alloc();

          __map( (void *)TEMP_PAGE, temp, 1 );
          memset( (void *)TEMP_PAGE, 0, PAGE_SIZE );
          __unmap( (void *)TEMP_PAGE );

          __map_page_table((void *)0x4000000, temp);

          lastTable = ((unsigned)pheader->vaddr + j * PAGE_SIZE) & 0xFFC00000;

          // mappingTable.setPTable( addrSpace, (void *)((unsigned)pheader->vaddr + j * PAGE_SIZE) );
          set_ptable_status(addrSpace, (void *)((unsigned)pheader->vaddr + j * PAGE_SIZE), true);
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

  tempPage = __unmap_page_table(image);
  free_phys_page(tempPage);

  return 0;
}


/* Handles exceptions, memory allocations, i/o permissions,
   DMA, address spaces, exit messages */
/*
struct MessageQueue servQueue;
struct Message mboxQueue[32];
*/

//unsigned int pointer=0, tail=0;
/*
unsigned short pointer = 0, tail = 0;
struct Message mboxQueue[16];
struct MessageQueue servQueue;

void initMsgQueue( void )
{
  servQueue.length = sizeof mboxQueue / sizeof(struct Message);
  servQueue.queueAddr = mboxQueue;
  servQueue.pointer = &pointer;
  servQueue.tail = &tail;
  servQueue.lock = 0;
}

int _allocate_mem_region(struct AllocMemInfo *info, tid_t sender)
{
  struct MemRegion *region = region_malloc();
  
  if( region == NULL )
    return -1;

  void *addr_space = lookup_tid(sender);

  region->start = (unsigned int)info->addr & 0xFFFFF000;
  region->length = info->pages * PAGE_SIZE;

  return attach_mem_region(addr_space, region);
}

void allocate_mem_region( struct Message *msg )
{
  struct UMPO_Header *header = (struct UMPO_Header *)msg->data;
  _allocate_mem_region((struct AllocMemInfo *)(header + 1), msg->sender);
}
*/
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
  void *physAddr;

  __get_thread_info( tid, &thread_info );

  if( thread_info.state.int_num == 14 )
  {
  /* Only accept if exception was caused by accessing a non-existant user page.
     Then check to make sure that the accessed page was allocated to the thread. */

    if ( (thread_info.state.error_code & 0x5) == 0x4 && find_address((void *)thread_info.addr_space, 
          (void *)cr2))
    {
      // Map in a new page table if not present.
      if( get_ptable_status((void *)thread_info.addr_space, (void *)(cr2 & ~0x3FFFFF)) == false )
      {
        physAddr = alloc_phys_page(NORMAL, (void *)thread_info.addr_space);

        __map((void *)TEMP_PAGE, (void *)physAddr, 1);
        memset((void *)TEMP_PAGE, 0, PAGE_SIZE);
        __unmap((void *)TEMP_PAGE);

        __map_page_table((void *)TEMP_PTABLE, physAddr);
        __grant_page_table((void *)TEMP_PTABLE, (void *)(cr2 & ~0x3FFFFF),(void *)thread_info.addr_space, 1);
        set_ptable_status((void *)thread_info.addr_space, (void *)(cr2 & ~0x3FFFFF), true);
      }
      physAddr = alloc_phys_page(NORMAL, (void *)thread_info.addr_space);//pageAllocator->alloc();

      __map( (void *)TEMP_PAGE, (void *)physAddr, 1 );

      __grant( (void *)TEMP_PAGE, (void *)(cr2 & ~0xFFF), (void *)thread_info.addr_space, 1 );

      __end_page_fault(thread_info.tid);
    }
    else if( (thread_info.state.error_code & 0x05) && (cr2 & ~0x3FFFFF) == STACK_TABLE ) /* XXX: This can be done better. Will not work if there aren't
                                                                                       any pages in the stack page! */
    {
      physAddr = alloc_phys_page(NORMAL, (void *)thread_info.addr_space);
      __map( (void *)TEMP_PAGE, (void *)physAddr, 1 );
      __grant( (void *)TEMP_PAGE, (void *)(cr2 & ~0xFFF), (void *)thread_info.addr_space, 1);
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

/*
void map_memory( struct Message *msg )
{
        struct UMPO_Header *header = (struct UMPO_Header *)msg->data;
        struct MapMemArgs *args = (struct MapMemArgs *)(header + 1);
        void *pdir = lookup_tid(msg->sender);
        
        _mapMem(args->phys, args->virt, args->pages, args->flags, pdir); // This needs to actually keep a record of the mapping also
}

void map_tid( struct Message *msg )
{
  struct UMPO_Header *header = (struct UMPO_Header *)msg->data;
  struct MapTidArgs *args = (struct MapTidArgs *)(header+1);

  if( args->addr == NULL_PADDR )
    attach_tid(lookup_tid(msg->sender), args->tid);
  else
    attach_tid(args->addr, args->tid);
}

void create_shared_mem( struct Message *msg )
{
  struct UMPO_Header *header = (struct UMPO_Header *)msg->data;   
  struct CreateShmArgs *args = (struct CreateShmArgs *)(header+1);    
  init_shmem( args->shmid, msg->sender, args->pages, args->ro_perm );
  shmem_attach( args->shmid, _lookup_tid(msg->sender), &args->region );
}

void attach_shared_mem( struct Message *msg )
{
  struct UMPO_Header *header = (struct UMPO_Header *)msg->data;   
  struct AttachShmRegArgs *args = (struct AttachShmRegArgs *)(header+1);
  shmem_attach( args->shmid, _lookup_tid(msg->sender), &args->region );
}

void detach_shared_mem( struct Message *msg )
{
  struct UMPO_Header *header = (struct UMPO_Header *)msg->data;   
  struct DetachShmRegArgs *args = (struct DetachShmRegArgs *)(header+1);
 // shmem_detach( args->shmid, _lookup_tid(msg->sender), &args->region );
}

void delete_shared_mem( struct Message *msg )
{
  struct UMPO_Header *header = (struct UMPO_Header *)msg->data;   
  struct DeleteShmRegArgs *args = (struct DeleteShmRegArgs *)(header+1);
//  shmem_delete( args->shmid, _lookup_tid(msg->sender), &args->region );
}
*/
void handle_message( void )
{
  volatile struct Message msg;
  int result = 0;
  int result2;
  tid_t sender;

  while( (result2=__receive( NULL_TID, (struct Message *)&msg, 0 )) == 2 );

  sender = msg.sender;

  if( msg.protocol == MSG_PROTO_GENERIC )
  {
    volatile struct GenericReq *req = (volatile struct GenericReq *)msg.data;

    if( req->request == MSG_REPLY )
    {
      print("Received a reply?\n");
      return;
    }

    switch( req->request )
    {
      case MAP_MEM:
      {
        void *pdir = lookup_tid(sender);
        _mapMem((void *)req->arg[0], (void *)req->arg[1], (unsigned)req->arg[2], req->arg[3], pdir); // doesn't return result

        break;
      }
      case ALLOC_MEM:
      {
        struct MemRegion *region = region_malloc();
        void *addr_space = lookup_tid(sender);

        if( region == NULL )
        {
          print("Couldn't allocate region\n");
     //   return; 
         result = -1;
         break;
        }

        region->start = (unsigned int)req->arg[0] & 0xFFFFF000;
        region->length = (size_t)req->arg[1] * PAGE_SIZE;

        result = attach_mem_region(addr_space, region);

        break;
      }
      case MAP_TID:
      {
        if( req->arg[1] == (int)NULL_PADDR )
          result = attach_tid(lookup_tid(sender), (tid_t)req->arg[0]);
        else
          result = attach_tid((void *)req->arg[1], (tid_t)req->arg[0]);
        break;
      }
      case CREATE_SHM:
      {
        struct MemRegion region;

        region.start = req->arg[3];
        region.length = req->arg[4];

        result = init_shmem( (shmid_t)req->arg[0], sender, (unsigned)req->arg[1], (bool)req->arg[2] );

        if( result == 0 )
          result = shmem_attach( (shmid_t)req->arg[0], _lookup_tid(sender), &region );
        break;
      }
      case ATTACH_SHM_REG:
      {
        struct MemRegion region;

        region.start = req->arg[3];
        region.length = req->arg[4];

        result = shmem_attach( (shmid_t)req->arg[0], _lookup_tid(sender), &region );
        break;
      }
      case DETACH_SHM_REG:
      {
 // detach_shared_mem( args->shmid, _lookup_tid(sender), &args->region );

        break;
      }
      case DELETE_SHM:
      {
  //delete_shared_mem( args->shmid, _lookup_tid(sender), &args->region );

        break;
      }
      case REGISTER_NAME:
      {
        tid_t t;

        if( (t=lookup_name((char *)&req->arg[1], req->arg[0])) == NULL_TID )
          result = register_name((char *)&req->arg[1], req->arg[0], sender);
        else
          result = -1;
        break;
      }
      case LOOKUP_NAME:
        result = lookup_name((char *)&req->arg[1], req->arg[0]);
        break;
/*      case SET_IO_PERM:
        result = set_io_perm(req->argv[0], req->argv[1], argv[2], sender);
        break; */
      default:
        print("Received bad request!![");
        printHex(req->request);
        print(", ");
        printHex(sender);
        print("]\n");
        break;
    }

    req->request = MSG_REPLY;
    req->arg[0] = result;
    msg.length = sizeof *req;

    while( __send( sender, (struct Message *)&msg, 0 ) == 2 );
  }
  else if( msg.protocol == MSG_PROTO_DEVMGR )
  {
    int status = REPLY_ERROR;
    int *type = (int *)msg.data;
    struct DevMgrReply *reply_msg = (struct DevMgrReply *)msg.data;

    switch( *type )
    {
      case DEV_REGISTER:
      {
        struct RegisterNameReq *req = (struct RegisterNameReq *)msg.data;
        int retval = registerDevName(sender, req->name, req->name_len, 
                       req->name_type, &req->entry);

        if( retval > 0 )
          status = REPLY_FAIL;
        else if( retval == 0 )
          status = REPLY_SUCCESS;
        else
          status = REPLY_ERROR;

        break;
      }
      case DEV_LOOKUP_NAME:
      case DEV_LOOKUP_MAJOR:
      {
        struct NameLookupReq *req = (struct NameLookupReq *)msg.data;
        struct Device *device = NULL;
        struct NameEntry *entry = NULL;

        if( *type == DEV_LOOKUP_MAJOR )
          device = _lookupMajor( req->major );
        else
          entry = _lookupName( req->name, req->name_len );

        if( device == NULL && entry == NULL )
          status = REPLY_ERROR;
        else
        {
          if( *type != DEV_LOOKUP_MAJOR )
          {
            reply_msg->entry = entry->entry;
            reply_msg->type = entry->name_type;
          }
          else
          {
            reply_msg->entry.device = *device;
            reply_msg->type = DEV_NAME;
          }
          status = REPLY_SUCCESS;
        }
        break;
      }
      default:
        break;
    }
    reply_msg->reply_status = status;
    msg.length = sizeof *reply_msg;

    while( __send( sender, (struct Message *)&msg, 0 ) == -2 );
  }
  else
  {
    print("Invalid message protocol: ");
    print(toIntString(msg.protocol));
    print(" ");
    print(toIntString(sender));
    print(" ");
    print(toIntString(msg.length));
    print("\n");
    return;
  }
}

void signal_handler(int signal, int arg)
{
  if( (signal & 0xFF) == SIGEXP )
  {
    tid_t tid = (signal >> 8) & 0xFFFF;
    handle_exception(tid, arg);    
  }
  else if( (signal & 0xFF) == SIGEXIT )
  {
    tid_t tid = (signal >> 8) & 0xFFFF;

    print("TID: "), printInt(tid), print(" exited\n");
  }
}

int main( int argc, char **argv )
{
  int i;
  allocEnd = (void *)0x2000000;
  availBytes = 0;
  sysID = 0;
  //tid_t tid;

  /* It's important not to cause an exception! */

  __map((void *)0xB8000, (void *)0xB8000, 8);

  get_boot_info(argc, argv);
  list_init(&shmem_list, list_malloc, list_free);

  set_signal_handler( &signal_handler );

  for(i=1; i < boot_info->num_mods; i++)
    load_elf_exec(&boot_modules[i], NULL);

  while(1)
    handle_message();
  return 0;
}
