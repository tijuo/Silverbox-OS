struct SharedRegion
{
  tid_t owner;
  shmid_t shmid;
  ListType phys_page_list;
  ListType pmem_region_list;
};

struct PMemRegion
{
  MemRegion region;
  struct AddrSpace *addr_space;
  bool rw;
};
