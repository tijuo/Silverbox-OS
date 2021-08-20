#ifndef PHYS_ALLOC
#define PHYS_ALLOC

#include "initsrv.h"
#include "paging.h"
#include <stddef.h>
#include <stdbool.h>

#if 0
#define NULL_PADDR      (void *)0xFFFFFFFF
#endif

struct PageList
{
  struct PhysPage *head, *tail;
};

struct PhysPage
{
  union
  {
    struct
    {
      unsigned int used : 1;
      unsigned int rd_only : 1;
      unsigned int copy_on_wr : 1;
      unsigned int type : 2;
      unsigned int no_swap : 1;
      unsigned int shared : 1;
      unsigned int rsvd : 5;
      unsigned int pdir_page : 20; // This is the address space that the page is in
    };
    unsigned int info;
  };
  struct PhysPage *prev;
  struct PhysPage *next;
};

enum PageType { NORMAL, CONV, DMA, KERNEL };

struct PhysPage *phys_page_list;
struct PageList free_lists[4];
struct PageList used_lists[4];
unsigned total_phys_pages;
extern struct MemoryArea *memory_areas;
extern unsigned int *page_dir;
extern struct BootInfo *boot_info;

void free_phys_page(addr_t address);
addr_t alloc_phys_page(enum PageType type, addr_t addr_space);
int init_page_lists(void *table_address, unsigned num_pages, void *start_used_addr, unsigned pages_used);

#endif /* PHYS_ALLOC_H */
