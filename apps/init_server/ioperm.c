#define TEMP_PAGE       0x10000

int enable_io( unsigned short from, unsigned short to, bool set, tid_t tid )
{
  addr_t phys = alloc_phys();
  addr_t phys2 = alloc_phys();

  // if the tid doesn't have an io permission bitmap, create one by allocating pages and mapping them into the permission bitmap area


  return __set_io_perm( from, to, set, tid );
}
