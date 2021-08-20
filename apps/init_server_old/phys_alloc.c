#include "phys_alloc.h"
#include <string.h>

static void _remove_phys_page(unsigned page_num, struct PageList *page_list);
static void _add_phys_page(unsigned page_num, struct PageList *page_list);
static int _alloc_phys_page(enum PageType type);
static int _free_phys_page(unsigned page_num);

int init_page_lists(void *table_address, unsigned num_pages, void *start_used_addr, unsigned pages_used)
{
  /* 'first_used_page' through 'last_used_page' are the pages that were allocated for the pager
     during initialization. */

  unsigned first_used_page = (unsigned)start_used_addr / PAGE_SIZE;
  unsigned last_used_page = ((unsigned)start_used_addr + pages_used * PAGE_SIZE) / PAGE_SIZE - 1;
  unsigned i, j;
  enum PageType page_type;
  int is_used;
  struct PhysPage *last_phys_page;

  phys_page_list = (struct PhysPage *)table_address;
  memset(phys_page_list, 0, num_pages * sizeof(struct PhysPage));

  // Initialize the free lists and used lists

  for(i=0; i < num_pages; i++)
  {
    if( i >= first_used_page && i <= last_used_page )
    {
      phys_page_list[i].used = 1;
      phys_page_list[i].type = NORMAL;
      phys_page_list[i].pdir_page = (unsigned)page_dir >> 12;
    }
    else
    {
      for(j=0; j < boot_info->num_mem_areas; j++)
      {
        if( (i >= memory_areas[j].base / PAGE_SIZE) && (i < memory_areas[j].base / PAGE_SIZE + memory_areas[j].length) )
        {
          phys_page_list[i].used = 0;
          phys_page_list[i].type = NORMAL;
          break;
        }
      }

      if( j == boot_info->num_mem_areas )
      {
        phys_page_list[i].used = 1;
        phys_page_list[i].type = KERNEL;
      }
    }

    page_type = phys_page_list[i].type;
    is_used = phys_page_list[i].used;

    if( is_used )
    {
      last_phys_page = used_lists[page_type].tail;

      if( used_lists[page_type].head == NULL )
        used_lists[page_type].head = &phys_page_list[i];
    }
    else
    {
      last_phys_page = free_lists[page_type].tail;

      if( free_lists[page_type].head == NULL )
        free_lists[page_type].head = &phys_page_list[i];
    }

    phys_page_list[i].prev = last_phys_page;
    phys_page_list[i].next = NULL;

    if( last_phys_page != NULL )
      last_phys_page->next = &phys_page_list[i];

    if( is_used )
      used_lists[page_type].tail = &phys_page_list[i];
    else
      free_lists[page_type].tail = &phys_page_list[i];
  }

  return 0;
}

static void _add_phys_page(unsigned page_num, struct PageList *page_list)
{
  struct PhysPage *page_ptr;

  page_ptr = &phys_page_list[page_num];
  page_ptr->next = page_ptr->prev = NULL;

  if( page_list->head == NULL )
    page_list->head = page_list->tail = page_ptr;
  else
  {
    page_list->tail->next = page_ptr;
    page_ptr->prev = page_list->tail;
    page_list->tail = page_ptr;
  }
}

static void _remove_phys_page(unsigned page_num, struct PageList *page_list)
{
  struct PhysPage *page_ptr;

  page_ptr = &phys_page_list[page_num];

  if( page_ptr->prev != NULL )
    page_ptr->prev->next = page_ptr->next;

  if( page_ptr->next != NULL )
    page_ptr->next->prev = page_ptr->prev;

  if( page_list->head == page_ptr )
    page_list->head = page_ptr->next;

  if( page_list->tail == page_ptr )
    page_list->tail = page_ptr->prev;

  page_ptr->next = page_ptr->prev = NULL;
}

static int _alloc_phys_page(enum PageType type)
{
  struct PhysPage *page_ptr = free_lists[type].head;
  unsigned page_num;

  if( page_ptr == NULL || page_ptr->used )
    return -1;

  page_num = page_ptr - phys_page_list;
  _remove_phys_page(page_num, &free_lists[type]);
  page_ptr->used = 1;
  _add_phys_page(page_num, &used_lists[type]);

  return page_num;
}

static int _free_phys_page(unsigned page_num)
{
  struct PhysPage *page_ptr;

  if( page_num >= total_phys_pages )
    return -1;

  page_ptr = &phys_page_list[page_num];

  if( page_ptr->used )
  {
    _remove_phys_page(page_num, &used_lists[page_ptr->type]);
    _add_phys_page(page_num, &free_lists[page_ptr->type]);
    page_ptr->used = 0;
    return 0;
  }
  else
    return -1;
}

addr_t alloc_phys_page(enum PageType type, addr_t addr_space)
{
  int page_num;

  if( type >= 4 || addr_space == NULL_PADDR )
  {
//    print("Bad ADDR space\n");
    return NULL_PADDR;
  }

  page_num = _alloc_phys_page(type);

  if( page_num == -1 )
    return NULL_PADDR;
  else
  {
    phys_page_list[page_num].pdir_page = (unsigned long)addr_space / PAGE_SIZE;
    return (page_num * PAGE_SIZE);
  }
}

void free_phys_page(addr_t address)
{

  unsigned page_num = (unsigned)address / PAGE_SIZE;
  _free_phys_page(page_num);
  phys_page_list[page_num].info = 0;
}
