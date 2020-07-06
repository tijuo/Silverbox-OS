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
  struct MemRegion virtRegion;
  struct MemRegion physRegion;
  int flags;
};

#include <os/bitmap.h>
#include <os/os_types.h>

struct AddrSpace
{
  addr_t phys_addr;
  SBArray memoryRegions;
  bitmap_t bitmap[NUM_PTABLES / 8];
};

struct Executable
{
  char *path;

  struct MemRegion codeRegion;
  struct MemRegion dataRegion;
  struct MemRegion bssRegion;
};

/*
struct ListNode *free_list_nodes;
struct ListType addr_space_list;
*/

//SBAssocArray addrSpaces; // phys_addr -> struct AddrSpace

void init_addr_space(struct AddrSpace *addr_space, addr_t phys_addr);
void delete_addr_space(struct AddrSpace *addr_space);
/*
int addAddrSpace(struct AddrSpace *addrSpace);
struct AddrSpace *lookupPhysAddr(void *phys_addr);
struct AddrSpace *removeAddrSpace(void *phys_addr);
*/
bool get_ptable_status(struct AddrSpace *addr_space, void *virt);
int set_ptable_status(struct AddrSpace *addr_space, void *virt, bool status);

//int attach_mem_region(void *aspace_phys, struct AddrRegion *region);
//bool region_overlaps(void *aspace_phys, struct MemRegion *region);
//bool find_address(void *aspace_phys, void *addr);

int attach_mem_region(struct AddrSpace *addr_space, struct AddrRegion *region);
bool region_overlaps(struct AddrSpace *addr_space, struct MemRegion *region);
bool find_address(struct AddrSpace *addr_space, void *addr);

#endif /* ADDR_SPACE_H */
