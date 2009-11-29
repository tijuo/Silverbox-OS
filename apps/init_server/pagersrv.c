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
#include "name.h"

/* Malloc works here as long as 'pager_addr_space' exists and is correct*/

#define STACK_TABLE	KVIRT_START
#define TEMP_PTABLE	0xE0000000
#define TEMP_PAGE	0x100000

/* FIXME: These should be passed to the init server by the kernel. */
#define KPHYS_START	0x1000000
#define KVIRT_START	0xFF400000

struct ProgramArgs
{
  char *data;
  int length;
};

/*
extern int registerThreadName(char *name, size_t len, tid_t tid);
extern tid_t lookupThreadName( char *name, size_t len);
*/

extern void print( char * );
extern void *list_malloc( size_t );
extern void list_free( void * );

extern void handle_exception( tid_t tid, int cr2 );
extern struct ListType shmem_list;

int extendHeap( unsigned pages );
int _mapMem( void *phys, void *virt, int pages, int flags, void *pdir );
void *_unmapMem( void *virt, void *pdir );
int mapMemRange( void *virt, int pages );

int get_boot_info( int argc, char **argv );

int load_elf_exec( struct BootModule *module, struct ProgramArgs *args );
bool isValidElfExe( void *img );
void handle_message(void);

void clearPage( void *page );

struct AddrSpace pager_addr_space;

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

  clearPage( (void *)start_page_addr );

  for(i=1; i < pages_needed+1; i++)
    clearPage( (void *)(start_page_addr + i * PAGE_SIZE) );

  __map_page_table(allocEnd, (void *)start_page_addr, 0, NULL_PADDR);

  __map(allocEnd, (void *)(start_page_addr + PAGE_SIZE), pages_needed, 0, NULL_PADDR);
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

/* This extends the end of the heap by a certain number of pages. */

int extendHeap( unsigned pages )
{
  void *virt;

  if ( (unsigned)allocEnd % PAGE_SIZE != 0 )
    virt = (void *)( ( (unsigned)allocEnd & 0xFFFFF000 ) + PAGE_SIZE  );
  else
    virt = allocEnd;

  return mapMemRange( virt, pages );
}

/* Maps a physical address range to a virtual address range in some address space. */

int _mapMem( void *phys, void *virt, int pages, int flags, void *pdir )
{
  unsigned char *addr = (unsigned char *)virt;
  void *table_phys;

  if( pages < 1 )
    return -1;

  while(pages--)
  {
    if( get_ptable_status((void *)pdir, addr) == false )
    {
      table_phys = alloc_phys_page(NORMAL, pdir);

      clearPage( table_phys );

      __map_page_table( addr, table_phys, 0, pdir );

      set_ptable_status(pdir, addr, true);
    }

//    phys = alloc_phys_page(NORMAL, pdir);

    // XXX: modify phys page attributes

    __map( addr, phys, 1, 0, pdir );

    addr = (void *)((unsigned)addr + PAGE_SIZE);
    phys = (void *)((unsigned)phys + PAGE_SIZE);
  }

  return 0;
}

void *_unmapMem( void *virt, void *pdir )
{
  // XXX: Need to clear the page table if it is empty!
  return __unmap( virt, pdir );
}

// Maps a virtual address to a physical address (in this address space)

int mapMemRange( void *virt, int pages )
{
  unsigned i=0;

  while( pages-- )
    _mapMem( alloc_phys_page(NORMAL, page_dir), 
             (void *)((unsigned)virt + i * PAGE_SIZE), 1, 0, page_dir );

  return 0;
}

void clearPage( void *page )
{
  if( __map((void *)TEMP_PAGE, page, 1, 0, NULL_PADDR) < 1 )
    print("Map failed.");

  memset((void *)TEMP_PAGE, 0, PAGE_SIZE);
  __unmap((void *)TEMP_PAGE, NULL_PADDR);
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

int loadExe( const char *path )
{
  elf_header_t header;
  elf_pheader_t pheader;
  unsigned pheader_count;
  tid_t tid;
  void *phys;
  void *addrSpace;
  struct AddrSpace *addr_space_struct;

  if( fatReadFile( path, 0, DEV_NUM(10, 0), &header, sizeof header ) < 0 )
    return -1;

  if( !isValidElfExe( &header ) )
    return -1;

  pheader_count = header.phnum;

  addrSpace = alloc_phys_page(NORMAL, page_dir);//pageAllocator->alloc();

  // XXX: Now create a 'struct AddrSpace' for the new thread

  addr_space_struct = malloc( sizeof( struct AddrSpace ) );
  init_addr_space(addr_space_struct, addrSpace);
  list_insert((int)addrSpace, addr_space_struct, &addr_space_list);

  clearPage( addrSpace );

  tid = __create_thread( (addr_t)header.entry, addrSpace, (void *)(TEMP_PTABLE + PTABLE_SIZE), 1 );

  if( tid == -1 )
  {
    free_phys_page( addrSpace );
    list_remove((int)addrSpace, &addr_space_list);
    free(addr_space_struct);
    return -1; // XXX: But first, free physical memory before returning
  }

  attach_tid(addrSpace, tid);

  for( unsigned i = 0; i < pheader_count; i++ )
  {
    fatReadFile( path, header.phoff + i * sizeof(pheader), DEV_NUM(10, 0), 
                 &pheader, sizeof pheader );

    if( pheader.type == PT_LOAD )
    {
      unsigned memSize = pheader.memsz;
      unsigned fileSize = pheader.filesz;

      for ( unsigned j = 0; memSize > 0; j++ )
      {
        phys = alloc_phys_page(NORMAL, addrSpace);

        if ( fileSize == 0 )
          clearPage(phys);
        else
        {
          __map( (void *)TEMP_PAGE, phys, 1, 0, page_dir );

          fatReadFile( path, pheader.offset + j * PAGE_SIZE, DEV_NUM(10,0),
                       (void *)TEMP_PAGE, PAGE_SIZE );
        }

        _mapMem( phys, (void *)(pheader.vaddr + j * PAGE_SIZE), 1, 0, addrSpace );

        if( fileSize > 0 )
          __unmap( (void *)TEMP_PAGE, page_dir );

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

  __start_thread( tid );
  return 0;
}

// !!! Requires physical pages for BSS !!! 

// Note that img is a physical address and needs to be mapped to an address somewhere

int load_elf_exec( struct BootModule *module, struct ProgramArgs *args )
{
  unsigned phtab_count, i, j;
  elf_header_t *image = (elf_header_t *)0xC0000000;//module->start_addr;
  elf_pheader_t *pheader;
  tid_t tid;
  void *phys;
  void *tempPage;
  size_t length = module->mod_end - module->mod_start;
  void *addrSpace, *temp;;
  unsigned lastTable = 1;
  struct AddrSpace *addr_space_struct;
  unsigned arg_len=8;

  if( module == NULL )
    return -1;

/* First, map in a page table for the image. */

  _mapMem( (void *)module->mod_start, image, (length % PAGE_SIZE == 0) ? (length / PAGE_SIZE) : (length / PAGE_SIZE) + 1, 0, page_dir );

/*
  tempPage = alloc_phys_page(NORMAL, page_dir);//pageAllocator->alloc();

  clearPage(tempPage);

  if( __map_page_table((void *)image, tempPage, 0, NULL_PADDR) < 0 )
  {
    free_phys_page(tempPage);
    print("__map_page_table() failed.\n");
   return -1;
  }

/ * Map the image to virtual memory. (assumes that the image is less than or equal to 4MB in size). * /

  if( __map((void *)image, (void *)module->mod_start, (length % PAGE_SIZE == 0 ? (length / PAGE_SIZE) : (length / PAGE_SIZE) + 1), 0, NULL_PADDR) < 1)
  {
    __unmap_page_table((void *)image, NULL_PADDR);
    free_phys_page(tempPage);
    print("__map failed!\n");
   return -1;
  }
*/

  if( !isValidElfExe( image ) )
  {
    for(i=0; i < (length % PAGE_SIZE == 0 ? (length / PAGE_SIZE) : (length / PAGE_SIZE) + 1); i++)
      __unmap((void *)((unsigned)image + i * PAGE_SIZE), NULL_PADDR);

    __unmap_page_table((void *)image, NULL_PADDR);
    print("Not a valid ELF executable.\n");
    return -1;
  }

  phtab_count = image->phnum;

  addrSpace = alloc_phys_page(NORMAL, page_dir);//pageAllocator->alloc();

  // XXX: Now create a 'struct AddrSpace' for the new thread

  addr_space_struct = malloc( sizeof( struct AddrSpace ) );
  init_addr_space(addr_space_struct, addrSpace);
  list_insert((int)addrSpace, addr_space_struct, &addr_space_list);

  clearPage( addrSpace );

  tempPage = alloc_phys_page(NORMAL, addrSpace);

  _mapMem( tempPage, (void *)(STACK_TABLE + PTABLE_SIZE - PAGE_SIZE), 1, 0, addrSpace );
  _mapMem( tempPage, (void *)(TEMP_PTABLE + PTABLE_SIZE - PAGE_SIZE), 1, 0, page_dir );

  if( args != NULL )
  {
    memcpy( (void *)(TEMP_PTABLE + PTABLE_SIZE - args->length - 8), args->data, args->length );
    arg_len += args->length;
  }

  memset( (void *)(TEMP_PTABLE + PTABLE_SIZE - 8), 0, 8 );
/*
  temp = alloc_phys_page(NORMAL, addrSpace);//pageAllocator->alloc();

  clearPage( temp );

  if( __map_page_table( (void *)TEMP_PTABLE, temp, 0, NULL_PADDR) < 0 )
    ;//cleanup;
 
  phys = alloc_phys_page(NORMAL, addrSpace);

  if( __map((void *)(TEMP_PTABLE + PTABLE_SIZE - PAGE_SIZE), phys, 1, 0, NULL_PADDR ) < 1 )
    ;// cleanup;

  if( args != NULL )
  {
    / * XXX: This may not work * /
    memcpy( (void *)(TEMP_PTABLE + PTABLE_SIZE - args->length - 8), args->data, args->length);
    arg_len += args->length;
  }

  memset( (void *)(TEMP_PTABLE + PTABLE_SIZE - 8), 0, 8);

  if( args != NULL )
  {
    ; // Here, put the start argument pointer and return address (if __exit() isn't called before program termination)
  }
*/

  _unmapMem( (void *)(TEMP_PTABLE + PTABLE_SIZE - PAGE_SIZE), NULL_PADDR );
  tid = __create_thread( (addr_t)image->entry, addrSpace, (void *)(TEMP_PTABLE + PTABLE_SIZE - arg_len), 1 );

  if( tid == -1 )
    return -1; // XXX: But first, free physical memory before returning

  attach_tid(addrSpace, tid); //mappingTable.map( tid, addrSpace );

//  __grant_page_table( (void *)TEMP_PTABLE, (void *)STACK_TABLE, addrSpace, 1 );
//  set_ptable_status( addrSpace, (void *)STACK_TABLE, true );

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
          phys = alloc_phys_page(NORMAL, addrSpace); //pageAllocator->alloc();
          clearPage( phys );
        }
        else
          phys = (void *)(pheader->offset + (unsigned)module->mod_start + j * PAGE_SIZE);

        _mapMem( phys, (void *)(pheader->vaddr + j * PAGE_SIZE), 1, 0, addrSpace );

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

  __start_thread( tid );

  for(i=0; i < (length % PAGE_SIZE == 0 ? (length / PAGE_SIZE) : (length / PAGE_SIZE) + 1); i++)
    _unmapMem( (void *)((unsigned)image + i * PAGE_SIZE), NULL_PADDR); //__unmap((void *)((unsigned)image + i * PAGE_SIZE), NULL_PADDR);

//  tempPage = __unmap_page_table(image, NULL_PADDR);
//  free_phys_page(tempPage);

  return 0;
}


/* Handles exceptions, memory allocations, i/o permissions,
   DMA, address spaces, exit messages */

/*

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
        struct AddrRegion *region;
        void *pdir;
        int flags = req->arg[3];

        pdir = lookup_tid(sender);

        if( pdir == NULL )
        {
          print("Request map_map(): Oops! TID ");
          printInt(sender);
          print(" is not registerd.");

          result = -1;
          break;
        }

        region = region_malloc();

        if( region == NULL )
        {
          print("Couldn't alloc region.");
          result = -1;
          break;
        }

        region->virtRegion.start = req->arg[1];
        region->physRegion.start = req->arg[0];
        region->virtRegion.length = region->physRegion.length = 
           req->arg[2]  * PAGE_SIZE;
        region->flags = MEM_MAP;

        if( flags & MEM_FLG_RO )
          region->flags |= MEM_RO;

        if( (flags & MEM_FLG_COW) && !(flags & MEM_FLG_ALLOC) ) // COW would imply !ALLOC
          region->flags |= MEM_RO | MEM_COW;

        if( flags & MEM_FLG_LAZY )	// I wonder how a lazy COW would work...
          region->flags |= MEM_LAZY;

        if( (flags & MEM_FLG_ALLOC) && !(flags & MEM_FLG_COW) ) // ALLOC would imply !COW
          region->flags &= ~MEM_MAP ;

        if( attach_mem_region(pdir, region) != 0 )
        {
          region_free(region);
          print("attach_mem_region() failed.");
          result = -1;
          break;
        }

        if( !(flags & MEM_FLG_LAZY) && !(flags & MEM_FLG_ALLOC) )
        {
          _mapMem((void *)req->arg[0], (void *)req->arg[1], 
                    (unsigned)req->arg[2], req->arg[3], pdir); // doesn't return result
        }

        result = 0;
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
        struct NameRecord *record = _lookupName((char *)&req->arg[1],req->arg[0], TID);

        if( !record )
          result = _registerName((char *)&req->arg[1],req->arg[0], TID, &sender);
        else
          result = -1;
        break;
      }
      case LOOKUP_NAME:
      {
        struct NameRecord *record = _lookupName((char *)&req->arg[1], req->arg[0], TID);

        if( record )
          result = record->entry.tid;
        else
          result = -1;
        break;
      }
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
        int retval;
        enum _NameType name_type;
        req->entry.device.ownerTID = sender;

        if( req->name_type == DEV_NAME )
          name_type = DEVICE;
        /*else if( name_type == FS_NAME )
          name_type = FS;*/
        else
        {
          status = REPLY_ERROR;
          break;
        }

        retval = _registerName(req->name, req->name_len, name_type, &req->entry);

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
        struct NameRecord *record = NULL;

        if( *type == DEV_LOOKUP_MAJOR )
          device = lookupDeviceMajor( req->major );
        else
          record = _lookupName( req->name, req->name_len, DEVICE );

        if( device == NULL && record == NULL )
          status = REPLY_ERROR;
        else
        {
          if( *type != DEV_LOOKUP_MAJOR )
          {
            reply_msg->entry.device = record->entry.device;

            if( record->name_type == DEVICE )
              reply_msg->type = DEV_NAME;
            else
              status = REPLY_ERROR;
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

//  __map((void *)0xB8000, (void *)0xB8000, 8, 0, NULL_PADDR);

  get_boot_info(argc, argv);
  list_init(&shmem_list, list_malloc, list_free);

  set_signal_handler( &signal_handler );

  for(i=1; i < boot_info->num_mods; i++)
    load_elf_exec(&boot_modules[i], NULL);

int t=0;

  while(1)
  {
    t++;
    handle_message();

//    if( t == 6 )
        //loadExe("/vidtest.exe");
  }
  return 0;
}
