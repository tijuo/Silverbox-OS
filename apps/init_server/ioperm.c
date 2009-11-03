#define TEMP_PAGE       0x10000

int enable_io( unsigned short from, unsigned short to, bool set, tid_t tid )
{
  addr_t phys = alloc_phys();
  addr_t phys2 = alloc_phys();

  // if the tid doesn't have an io permission bitmap, create one by allocating pages and mapping them into the permission bitmap area


  // assumes that the TSS isn't already mapped and it's not recorded

  if( !find_address( TSS_IO_PERM_BMP, lookup_tid(tid) ) )
  {
    __map( TEMP_PAGE, phys, 1 );
    memset( TEMP_PAGE, 0xFF, 4096 );
    __grant( TEMP_PAGE, TSS_IO_PERM_BMP, 1, tid );

    // XXX: record the first part of the io bitmap

    __map( TEMP_PAGE, phys2, 1 );
    memset( TEMP_PAGE, 0xFF, 4096 );
    __grant( TEMP_PAGE, TSS_IO_PERM_BMP + 4096, 1, tid );

    // XXX: record the second part of the io bitmap
/*
    if( __grant( TEMP_PAGE, TSS_IO_PERM_BMP, 1, tid ) < 0 ) // already has a io perm bitmap
    {
      free_phys(phys);
      free_phys(phys2);
    }
    else
    {
      __map( TEMP_PAGE, phys2, 1 );
      memset( TEMP_PAGE, 0xFF, 4096 );

      if( __grant( TEMP_PAGE, TSS_IO_PERM_BMP + 4096, 1, tid ) < 0 )
        free_phys(phys2);
    }
*/
  }

  return __set_io_perm( from, to, set, tid );
}
