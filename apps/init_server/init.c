#include "initsrv.h"
#include "name.h"
#include <oslib.h>
#include <os/elf.h>
#include <os/signal.h>
#include "paging.h"
#include "phys_alloc.h"
#include "shmem.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern SBAssocArray mountTable;

static int get_boot_info( int argc, char **argv );
int init( int argc, char **argv );
void signal_handler(int signal, int arg);
static int load_elf_exec( struct BootModule *module, struct ProgramArgs *args );

extern void handle_exception( tid_t tid, unsigned int cr2 );

static int get_boot_info( int argc, char **argv )
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

  /* Get the total number of pages in the system. */

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

  /* Find out how many pages we need for everything. */

  ptr = (char *)allocEnd;

  bytes_to_allocate = total_pages * sizeof(struct PhysPage)/*length * 4 + 3 * sizeof(PageAllocator)*/ + 
     strlen(server_name) + 1 + sizeof(struct BootInfo) + 
     sizeof(struct MemoryArea) * boot_info->num_mem_areas +
     sizeof(struct BootModule) * boot_info->num_mods;

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

  unsigned tables_needed = pages_needed / 1024;
  unsigned addr, vAddr;

  if( pages_needed % 1024 )
    tables_needed++;

  for(i=0, addr=start_page_addr; i < tables_needed + pages_needed; i++,
      addr += PAGE_SIZE)
  {
    clearPage( (void *)addr );
  }

  for(i=0, addr=start_page_addr, vAddr=allocEnd; i < tables_needed; i++, 
      addr += PAGE_SIZE)
  {
    __map_page_table((void *)vAddr, (void *)addr, 0, NULL_PADDR);
  }

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

  initsrv_pool.addrSpace.phys_addr = page_dir;
  initsrv_pool.execInfo = NULL;
  initsrv_pool.id = 1;

  initsrv_pool.ioBitmaps.phys1 = alloc_phys_page(NORMAL, page_dir);
  initsrv_pool.ioBitmaps.phys2 = alloc_phys_page(NORMAL, page_dir);
  init_addr_space(&initsrv_pool.addrSpace, page_dir);

  for(i=0; i < bytes_to_allocate; i += PTABLE_SIZE)
    set_ptable_status(&initsrv_pool.addrSpace, (void *)(temp + i), true);

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
  void *addrSpace;
  struct ResourcePool *newPool;
  unsigned arg_len=8;
  FILE *file = fopen(filename, "r");
print("a");
  if( !file )
    return -1;

  if( fread(&image, sizeof image, 1, file) != sizeof image )
  {
    fclose(file);
    return -1;
  }
print("b");
  /* Create an address space for the new exe */

  newPool = create_resource_pool();

  addrSpace = newPool->addrSpace.phys_addr;

  if( !isValidElfExe( &image ) )
  {
    fclose(file);
    print("Not a valid ELF executable.\n");
    return -1;
  }
print("c");
  /* Map the stack in the current address space, so the program arguments can
     be placed there. */

  phys = alloc_phys_page(NORMAL, addrSpace);

  _mapMem( phys, (void *)(STACK_TABLE + PTABLE_SIZE - PAGE_SIZE), 1, 0, &newPool->addrSpace );
  _mapMem( phys, (void *)(TEMP_PTABLE + PTABLE_SIZE - PAGE_SIZE), 1, 0, &initsrv_pool.addrSpace );

  if( args != NULL )
  {
    memcpy( (void *)(TEMP_PTABLE + PTABLE_SIZE - strlen(args) - 8), args, strlen(args) );
    arg_len += strlen(args);
  }

  memset( (void *)(TEMP_PTABLE + PTABLE_SIZE - 8), 0, 8 );

  _unmapMem( (void *)(TEMP_PTABLE + PTABLE_SIZE - PAGE_SIZE), NULL );
print("d");
  tid = __create_thread( (addr_t)image.entry, addrSpace, (void *)(STACK_TABLE + PTABLE_SIZE - arg_len), 1 );

  if( tid == -1 )
  {
    free_phys_page(phys);
    destroy_resource_pool(newPool);
    fclose(file);
    return -1; // XXX: But first, free physical memory before returning
  }

  attach_tid(newPool, tid); //mappingTable.map( tid, addrSpace );
  attach_phys_aspace(newPool, addrSpace);
  attach_resource_pool(newPool);
print("e");
  tempPage = malloc(PAGE_SIZE);

  if( !tempPage )
  {
    free_phys_page(phys);
    destroy_resource_pool(newPool);
    fclose(file);
    return -1; // XXX: But first, free physical memory before returning
  }

  for ( i = 0; i < image.phnum; i++ )
  {
    print("Reading pheader\n");
    fseek(file, image.phoff+i*sizeof pheader, SEEK_SET);

    if( fread(&pheader, sizeof pheader, 1, file) != sizeof pheader )
    {
      print("Error reading ELF file\n");
      return -1;
    }

    if ( pheader.type == PT_LOAD )
    {
      unsigned memSize = pheader.memsz;
      unsigned fileSize = pheader.filesz;

      for ( j = 0; memSize > 0; j++ )
      {
        phys = alloc_phys_page(NORMAL, addrSpace);

        if ( fileSize == 0 )
          clearPage( phys );
        else
        {
          fseek(file, pheader.offset + j * PAGE_SIZE, SEEK_SET);
          fread(tempPage, 1, PAGE_SIZE, file);
          pokePage(phys, tempPage);
        }

        _mapMem( phys, (void *)(pheader.vaddr + j * PAGE_SIZE), 1, pheader.flags & PF_W ? 0 : MEM_RO, &newPool->addrSpace );

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

static int load_elf_exec( struct BootModule *module, struct ProgramArgs *args )
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
  struct ResourcePool *newPool;
  unsigned arg_len=8;

  if( module == NULL )
    return -1;

  /* Create an address space for the new exe */

  newPool = create_resource_pool();

  addrSpace = newPool->addrSpace.phys_addr;

/* Map in a page table for the image. */

  _mapMem( (void *)module->mod_start, image, (length % PAGE_SIZE == 0) ? (length / PAGE_SIZE) : (length / PAGE_SIZE) + 1, 0, &initsrv_pool.addrSpace );

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

  /* Map the stack in the current address space, so the program arguments can
     be placed there. */

  tempPage = alloc_phys_page(NORMAL, addrSpace);

  _mapMem( tempPage, (void *)(STACK_TABLE + PTABLE_SIZE - PAGE_SIZE), 1, 0, &newPool->addrSpace );
  _mapMem( tempPage, (void *)(TEMP_PTABLE + PTABLE_SIZE - PAGE_SIZE), 1, 0, &initsrv_pool.addrSpace );

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

  _unmapMem( (void *)(TEMP_PTABLE + PTABLE_SIZE - PAGE_SIZE), NULL );

  tid = __create_thread( (addr_t)image->entry, addrSpace, (void *)(STACK_TABLE + PTABLE_SIZE - arg_len), 1 );

  if( tid == -1 )
    return -1; // XXX: But first, free physical memory before returning

  attach_tid(newPool, tid); //mappingTable.map( tid, addrSpace );
  attach_phys_aspace(newPool, addrSpace);
  attach_resource_pool(newPool);

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

        _mapMem( phys, (void *)(pheader->vaddr + j * PAGE_SIZE), 1, pheader->flags & PF_W ? 0 : MEM_RO, &newPool->addrSpace );

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
    _unmapMem( (void *)((unsigned)image + i * PAGE_SIZE), NULL); //__unmap((void *)((unsigned)image + i * PAGE_SIZE), NULL_PADDR);

//  tempPage = __unmap_page_table(image, NULL_PADDR);
//  free_phys_page(tempPage);

  return 0;
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
    detach_tid(tid);
    // XXX: Unregister any associated names or devices
  }
}

int init(int argc, char **argv)
{
  allocEnd = (void *)0x2000000;
  availBytes = 0;
  sysID = 0;

  get_boot_info(argc, argv);
  sbAssocArrayCreate(&deviceTable, 256);
  sbAssocArrayCreate(&fsNames, 256);
  sbAssocArrayCreate(&fsTable, 256);
  sbAssocArrayCreate(&deviceNames, 256);
  sbAssocArrayCreate(&threadNames, 256);  // XXX: May need to do an update operation on full
  sbAssocArrayCreate(&mountTable, 256);

  //list_init(&shmem_list, list_malloc, list_free);

  set_signal_handler( &signal_handler );

  for(int i=1; i < boot_info->num_mods; i++)
    load_elf_exec(&boot_modules[i], NULL);

  return 0;
}
