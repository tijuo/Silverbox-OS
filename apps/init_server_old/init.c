#include "initsrv.h"
#include "name.h"
#include <oslib.h>
#include <os/elf.h>
#include "paging.h"
#include "phys_alloc.h"
#include "shmem.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <os/multiboot.h>
#include <os/syscalls.h>

//extern SBAssocArray mountTable;
extern int _readFile(const char *, int, void *, size_t);

// Extracts boot params passed by the kernel

#if 0
static int get_boot_info( int argc, char **argv );
#endif /* 0 */
static int load_elf_exec( struct BootModule *module, struct ProgramArgs *args );

extern void handle_exception( tid_t tid, unsigned int cr2 );
int init(multiboot_info_t *info, addr_t lastKernelFreePage);

mutex_t print_lock=0;

void printC( char c )
{
  char volatile *vidmem = (char *)(0xB8000 + 160 * 4);
  static int i=0;

  while(mutex_lock(&print_lock))
    sys_wait(0);

  if( c == '\n' )
    i += 160 - (i % 160);
  else
  {
    vidmem[i++] = c;
    vidmem[i++] = 7;
  }

  mutex_unlock(&print_lock);
}

void printN( char *str, int n )
{
  for( int i=0; i < n; i++, str++ )
    printC(*str);
}

void print( char *str )
{
  for(; *str; str++ )
    printC(*str);
}

void printInt( int n )
{
  print(toIntString(n));
}

void printHex( int n )
{
  print(toHexString(n));
}

/*
  multiboot_info_t
  free page start (assume all available memory above this is free)

  need to know which page frames are reserved this can be done by
  mapping our page directory
*/

static int get_boot_info(multiboot_info_t *info, addr_t lastKernelFreePage)
{
  // map(info)

  // for every page in the available region, if the address is not
  // in the kernel, not in low memory, or if it is discarded then
  // add it to the free page stack
}

#if 0
static int get_boot_info( int argc, char **argv )
{
  unsigned int bytes_to_allocate=0;
  unsigned int i, pages_needed=0, start_page_addr, max_mem_addr, max_mem_length;
  unsigned temp, tables_needed, addr, vAddr;
  char *ptr;

  server_name = argv[0];
  boot_info = (struct BootInfo *)argv[1];
  paddr_t lastKernelPage = (paddr_t *)argv[2];
  //memory_areas = (struct MemoryArea *)argv[2];
  boot_modules = (struct BootModule *)argv[3];
  page_dir = (unsigned int *)argv[4];

  /* Get the total number of pages in the system. */
/*
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
*/

  /* Find out how many pages we need for everything. */

  ptr = (char *)allocEnd;

  bytes_to_allocate = total_pages * sizeof(struct PhysPage)/*length * 4 + 3 * sizeof(PageAllocator)*/ +
     strlen(server_name) + 1 + sizeof(struct BootInfo) +
     sizeof(struct MemoryArea) * boot_info->num_mem_areas +
     sizeof(struct BootModule) * boot_info->num_mods;

  pages_needed = bytes_to_allocate / PAGE_SIZE + bytes_to_allocate % PAGE_SIZE;
  tables_needed = pages_needed / 1024;

  if( pages_needed % 1024 )
    tables_needed++;

  for(i=0; i < boot_info->num_mem_areas; i++)
  {
    if( memory_areas[i].base >= KPHYS_START && memory_areas[i].length >= pages_needed + tables_needed )
    {
      start_page_addr = memory_areas[i].base;
      break;
    }
  }

  struct SysUpdateMapping args;

  for(i=0, addr=start_page_addr, vAddr=allocEnd; i < tables_needed; i++,
      addr += PAGE_SIZE, vAddr += PTABLE_SIZE)
  {
    clearPage((void *)addr);
    args.vaddr = (void *)vAddr;
    args.paddr = (void *)addr;
    args.level = 0;
    args.addr_space = (void *)NULL_PADDR;
    args.flags = 0;

    sys_update(RES_MAPPING, &args);

//    __map_page_table((void *)vAddr, (void *)addr, 0, NULL_PADDR);
  }
/*
  for(i=0, addr=start_page_addr+tables_needed*PAGE_SIZE; i < pages_needed; i++,
      addr += PAGE_SIZE)
  {
    clearPage( (void *)addr );
  }
*/
  __map(allocEnd, (void *)(start_page_addr + tables_needed * PAGE_SIZE),
        pages_needed, 0, NULL_PADDR);

  pages_needed += tables_needed;

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

  initsrv_pool.addr_space.phys_addr = page_dir;
//  initsrv_pool.execInfo = NULL;
  initsrv_pool.id = 1;

  initsrv_pool.ioBitmaps.phys1 = alloc_phys_page(NORMAL, page_dir);
  initsrv_pool.ioBitmaps.phys2 = alloc_phys_page(NORMAL, page_dir);
  addr_space_init(&initsrv_pool.addr_space, page_dir);

  for(i=0; i < bytes_to_allocate; i += PTABLE_SIZE)
    set_ptable_status(&initsrv_pool.addr_space, (void *)(temp + i), true);

  sbAssocArrayCreate(&physAspaceTable, 512); // XXX: May need to do an update operation on full
  sbAssocArrayCreate(&tidTable, 512); // XXX: May need to do an update operation on full
  sbAssocArrayCreate(&resourcePools, 512);  // XXX: May need to do an update operation on full

  sbArrayCreate(&initsrv_pool.tids);

  setPage(initsrv_pool.ioBitmaps.phys1, 0xFF);
  setPage(initsrv_pool.ioBitmaps.phys2, 0xFF);

  attach_phys_aspace(&initsrv_pool, page_dir);
  attach_tid(&initsrv_pool, 1);
  attach_resource_pool(&initsrv_pool);

  return 0;
}

int loadElfFile( char *filename, char *args )
{
  unsigned i, j;
  elf_header_t image;
  elf_pheader_t pheader;
  tid_t tid;
  void *phys;
  void *tempPage;
  void *addr_space;
  struct ResourcePool *newPool;
  unsigned arg_len=8;

  if( _readFile(filename, 0, &image, sizeof image) != sizeof image )
    return -1;

  /* Create an address space for the new exe */

  newPool = create_resource_pool();

  addr_space = newPool->addr_space.phys_addr;

  if( !is_valid_elf_exe( &image ) )
  {
    print("Not a valid ELF executable.\n");
    return -1;
  }

  /* Map the stack in the current address space, so the program arguments can
     be placed there. */

  phys = alloc_phys_page(NORMAL, addr_space);

  _mapMem( phys, (void *)(STACK_TABLE + PTABLE_SIZE - PAGE_SIZE), 1, 0, &newPool->addr_space );
  _mapMem( phys, (void *)(TEMP_PTABLE + PTABLE_SIZE - PAGE_SIZE), 1, 0, &initsrv_pool.addr_space );

  if( args != NULL )
  {
    memcpy( (void *)(TEMP_PTABLE + PTABLE_SIZE - strlen(args) - 8), args, strlen(args) );
    arg_len += strlen(args);
  }

  memset( (void *)(TEMP_PTABLE + PTABLE_SIZE - 8), 0, 8 );

  _unmapMem( (void *)(TEMP_PTABLE + PTABLE_SIZE - PAGE_SIZE), NULL );

  tid = sys_create_thread( (addr_t)image.entry, addr_space, (void *)(STACK_TABLE + PTABLE_SIZE - arg_len), 1 );

  if( tid == NULL_TID )
  {
    print("Bad TID\n");
    free_phys_page(phys);
    destroy_resource_pool(newPool);
    return -1; // XXX: But first, free physical memory before returning
  }

  attach_tid(newPool, tid); //mappingTable.map( tid, addr_space );
  attach_phys_aspace(newPool, addr_space);
  attach_resource_pool(newPool);

  tempPage = malloc(PAGE_SIZE);

  if( !tempPage )
  {
    print("Can't alloc temp page\n");
    free_phys_page(phys);
    destroy_resource_pool(newPool);
    return -1; // XXX: But first, free physical memory before returning
  }

  for ( i = 0; i < image.phnum; i++ )
  {
    if( _readFile(filename, image.phoff+i*sizeof pheader, &pheader, sizeof pheader) != sizeof pheader )
    {
      print("Can't read pheader\n");
      return -1;
    }

    if ( pheader.type == PT_LOAD )
    {
      unsigned mem_size = pheader.memsz;
      unsigned file_size = pheader.filesz;

      for ( j = 0; mem_size > 0; j++ )
      {
        phys = alloc_phys_page(NORMAL, addr_space);

        if ( file_size == 0 )
          clearPage( phys );
        else
        {
          int num_read=0;
          if( (num_read=_readFile(filename, pheader.offset + j * PAGE_SIZE, tempPage, PAGE_SIZE)) != PAGE_SIZE )
          {
            print("Can't read program page. Only read "), printInt(num_read), print(" bytes!\n");
            return -1;
          }
          pokePage(phys, tempPage);
        }

        _mapMem( phys, (void *)(pheader.vaddr + j * PAGE_SIZE), 1, /*pheader.flags & PF_W ? 0 : MEM_RO*/ 0, &newPool->addr_space );

        if( mem_size < PAGE_SIZE )
          mem_size = 0;
        else
          mem_size -= PAGE_SIZE;

        if( file_size < PAGE_SIZE )
          file_size = 0;
        else
          file_size -= PAGE_SIZE;
      }
    }
  }

  struct SyscallReadTcbArgs args;

  sys_read(RES_TCB, &args);
  args.tcb->status = TCB_STATUS_READY;
  sys_update(RES_TCB, &args);

  return 0;
}
#endif /* 0 */

static int load_elf_exec( struct BootModule *module, struct ProgramArgs *args )
{
  unsigned phtab_count, i, j;
  elf_header_t *image = (elf_header_t *)0x40000000;//module->start_addr;
  elf_pheader_t *pheader;
  tid_t tid;
  void *phys;
  void *tempPage;
  size_t length = module->mod_end - module->mod_start;
  void *addr_space, *temp;;
  unsigned lastTable = 1;
  struct ResourcePool *newPool;
  unsigned arg_len=8;

  if( module == NULL )
    return -1;

  /* Create an address space for the new exe */

  newPool = create_resource_pool();

  addr_space = newPool->addr_space.phys_addr;

/* Map in a page table for the image. */

  _mapMem( (void *)module->mod_start, image, (length % PAGE_SIZE == 0) ? (length / PAGE_SIZE) : (length / PAGE_SIZE) + 1, 0, &initsrv_pool.addr_space );

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
#if 0
  if( !is_valid_elf_exe( image ) )
  {
    for(i=0; i < (length % PAGE_SIZE == 0 ? (length / PAGE_SIZE) : (length / PAGE_SIZE) + 1); i++)
      __unmap((void *)((unsigned)image + i * PAGE_SIZE), NULL_PADDR);

    __unmap_page_table((void *)image, NULL_PADDR);
    print("Not a valid ELF executable.\n");
    return -1;
  }
#endif /* 0 */

  phtab_count = image->phnum;

  /* Map the stack in the current address space, so the program arguments can
     be placed there. */

  tempPage = alloc_phys_page(NORMAL, addr_space);

  _mapMem( tempPage, (void *)(STACK_TABLE + PTABLE_SIZE - PAGE_SIZE), 1, 0, &newPool->addr_space );
  _mapMem( tempPage, (void *)(TEMP_PTABLE + PTABLE_SIZE - PAGE_SIZE), 1, 0, &initsrv_pool.addr_space );

  if( args != NULL )
  {
    memcpy( (void *)(TEMP_PTABLE + PTABLE_SIZE - args->length - 8), args->data, args->length );
    arg_len += args->length;
  }

  memset( (void *)(TEMP_PTABLE + PTABLE_SIZE - 8), 0, 8 );
/*
  temp = alloc_phys_page(NORMAL, addr_space);//pageAllocator->alloc();

  clearPage( temp );

  if( __map_page_table( (void *)TEMP_PTABLE, temp, 0, NULL_PADDR) < 0 )
    ;//cleanup;

  phys = alloc_phys_page(NORMAL, addr_space);

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

  _unmapMem( (void *)(TEMP_PTABLE + PTABLE_SIZE - PAGE_SIZE), NULL );

  struct SyscallCreateTcbArgs createArgs;

  createArgs.entry = (addr_t)image->entry;
  createArgs.addr_space = addr_space;
  createArgs.stack = (STACK_TABLE + PTABLE_SIZE - arg_len);
  createArgs.exHandler = INIT_SERVER;

  tid = (tid_t)sys_create(RES_TCB, &createArgs);

  if( tid == NULL_TID )
    return -1; // XXX: But first, free physical memory before returning

  attach_tid(newPool, tid); //mappingTable.map( tid, addr_space );
  attach_phys_aspace(newPool, addr_space);
  attach_resource_pool(newPool);

//  __grant_page_table( (void *)TEMP_PTABLE, (void *)STACK_TABLE, addr_space, 1 );
//  set_ptable_status( addr_space, (void *)STACK_TABLE, true );

  // Program header information is loaded into memory

  pheader = (elf_pheader_t *)((unsigned)image + image->phoff);

  for ( i = 0; i < phtab_count; i++, pheader++ )
  {
    if ( pheader->type == PT_LOAD )
    {
      unsigned mem_size = pheader->memsz;
      unsigned file_size = pheader->filesz;

      for ( j = 0; mem_size > 0; j++ )
      {
        if ( file_size == 0 )
        {
          phys = alloc_phys_page(NORMAL, addr_space); //pageAllocator->alloc();
          clearPage( phys );
        }
        else
          phys = (void *)(pheader->offset + (unsigned)module->mod_start + j * PAGE_SIZE);

        _mapMem( phys, (void *)(pheader->vaddr + j * PAGE_SIZE), 1, /*pheader->flags & PF_W ? 0 : MEM_RO*/0, &newPool->addr_space );

        if( mem_size < PAGE_SIZE )
          mem_size = 0;
        else
          mem_size -= PAGE_SIZE;

        if( file_size < PAGE_SIZE )
          file_size = 0;
        else
          file_size -= PAGE_SIZE;
      }
    }
  }

  struct SyscallReadTcbArgs tcbArgs;

  sys_read(RES_TCB, &tcbArgs);
  tcbArgs.tcb->status = TCB_STATUS_READY;
  sys_update(RES_TCB, &tcbArgs);

  for(i=0; i < (length % PAGE_SIZE == 0 ? (length / PAGE_SIZE) : (length / PAGE_SIZE) + 1); i++)
    _unmapMem( (void *)((unsigned)image + i * PAGE_SIZE), NULL); //__unmap((void *)((unsigned)image + i * PAGE_SIZE), NULL_PADDR);

//  tempPage = __unmap_page_table(image, NULL_PADDR);
//  free_phys_page(tempPage);

  return 0;
}

int init(multiboot_info_t *info, addr_t lastKernelFreePage)
{
  allocEnd = (void *)0x2000000;
  availBytes = 0;
  sysID = 0;

  if( get_boot_info(info, lastKernelFreePage) )
    return -1;

#if 0
  get_boot_info(argc, argv);
#endif /* 0 */
//  while(1)
//    sys_wait(1000);

  sbAssocArrayCreate(&device_table, 256);
  sbAssocArrayCreate(&fs_names, 256);
  sbAssocArrayCreate(&fs_table, 256);
  sbAssocArrayCreate(&device_names, 256);
  sbAssocArrayCreate(&thread_names, 256);  // XXX: May need to do an update operation on full
//  sbAssocArrayCreate(&mountTable, 256);

  //list_init(&shmem_list, list_malloc, list_free);

  for(int i=1; i < boot_info->num_mods; i++)
    load_elf_exec(&boot_modules[i], NULL);

  return 0;
}
