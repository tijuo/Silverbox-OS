#include <types.h>
#include <elf.h>
#include <os/progload.h>

bool isValidElfExe(elf_header_t *image) {
  if(!VALID_ELF(image))
    return false;
  else {
    if(image->identifier[ EI_VERSION] != EV_CURRENT)
      return false;
    if(image->type != ET_EXEC)
      return false;
    if(image->machine != EM_386)
      return false;
    if(image->version != EV_CURRENT)
      return false;
    if(image->identifier[ EI_CLASS] != ELFCLASS32)
      return false;
  }

  return true;
}

/*  */
/*
 int load_elf_exec( void *image, struct ProgramArgs *args )
 {
 elf_header_t *elf_img = image;

 // 1. Request an address space (or use existing one)
 // 2. Map in the code and data segments
 // 3. Create and zero a BSS segment
 // 4. Allocate stack memory
 // 5. Put input arguments on stack
 // 6. Create a new thread (and start it)

 if( !isValidElfExe(elf_img) )
 return -1;
 }
 */

/*
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
 */
