#ifndef ADDR_SPACE_H
#define ADDR_SPACE_H

#include <os/region.h>
#include <stddef.h>
#include <oslib.h>

#define REG_RO		1	// Read-only memory range
#define REG_COW		2	/* Range is copy-on-write (if written to, copy 
				   range to another set of pages & mark that
				   range as read-write [implies read-only]). */
#define REG_LAZY	4	// Do not immediately map the range into memory
#define REG_MAP		8	/* Map virtual addresses to a specific physical 
                                   addresses (as opposed to mapping virtual 
				   addresses to any physical addresses. */
#define REG_ZERO	16	// Clear an address range before mapping it.
#define REG_RESD	0xF0000000 // Range is reserved and shouldn't be accessed. For internal use only.

#define NUM_PTABLES     1024

#define REG_NOEXIST	0
#define REG_PHYS	1
#define REG_FILE	2

#define IS_PNULL(x)	((void *)x == (void *)NULL_PADDR)
#define IS_NULL(x)	((void *)x == (void *)NULL)

#define EXEC_RD_ONLY
#define EXEC_ZERO

/*
  Addresses in an address space can be non-existant, correspond to physical
  memory, or file. An address region may be swapped out to disk.

*/

struct AddrRegion
{
  MemRegion virt_region;
  MemRegion physRegion;
  int flags;
};

#include <os/bitarray.h>

struct AddrSpace
{
  addr_t phys_addr;
  DynArray memory_regions;
  bitarray_t bitarray[NUM_PTABLES / 8];
};

struct Executable
{
  char *path;

  MemRegion code_region;
  MemRegion data_region;
  MemRegion bss_region;
};

/*
struct ListNode *free_list_nodes;
struct ListType addr_space_list;
*/

//SBAssocArray addr_spaces; // phys_addr -> struct AddrSpace

void addr_space_init(struct AddrSpace *addr_space, addr_t phys_addr);
void delete_addr_space(struct AddrSpace *addr_space);

bool get_ptable_status(struct AddrSpace *addr_space, void *virt);
int set_ptable_status(struct AddrSpace *addr_space, void *virt, bool status);

int attach_mem_region(struct AddrSpace *addr_space, struct AddrRegion *region);
bool addr_space_region_intersects(struct AddrSpace *addr_space, MemRegion *region);
bool find_address(struct AddrSpace *addr_space, void *addr);

#endif /* ADDR_SPACE_H */
