#include <types.h>
#include <elf.h>

bool is_valid_elf_exe( void *img )
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

/*  */

int load_elf_exec( struct BootModule *module, struct ProgramArgs *args )
{
  // 1. Request an address space (or use existing one)
  // 2. Map in the code and data segments
  // 3. Create and zero a BSS segment
  // 4. Allocate stack memory
  // 5. Put input arguments on stack
  // 6. Create a new thread (and start it)


}
