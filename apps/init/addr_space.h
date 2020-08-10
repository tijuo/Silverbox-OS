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
#define REG_NOEXEC	32	// Do not allow code execution on memory range

#define REG_RESD	0xF0000000 // Range is reserved and shouldn't be accessed. For internal use only.

#define NUM_PTABLES     1024

#define REG_NOEXIST	0
#define REG_PHYS	1
#define REG_FILE	2

#define IS_PNULL(x)	((void *)x == (void *)NULL_PADDR)
#define IS_NULL(x)	((void *)x == (void *)NULL)

#define EXEC_RD_ONLY
#define EXEC_ZERO

#define PAGE_RO			1	// read-only
#define PAGE_WT			2	// write-through
#define PAGE_WB			4	// write-back
#define PAGE_NO_SWAP		8	// Unswappable page

/* A page can either exist in memory or on disk. Pages can originate in RAM or disk.
   Pages can be swapped from RAM to disk upon low memory conditions. */

typedef struct Page
{
  int device;
  unsigned int block;
  unsigned int offset;
  int lastAccessed;

  pframe_t physFrame  : 20;
  int      isOnDisk   :  1;
  int      isDirty    :  1;
  int      isDiskPage :  1;
  int      flags      :  9;
} page_t;

/*
  Address region are areas of the address space that can be marked with various flags. Upon
  a page fault, the memory regions are checked for their flags and be mapped appropriately,
  if no protection fault has occurred.

*/

struct AddrRegion
{
  struct MemRegion virtRegion;
  int flags;
};

#include <os/bitmap.h>
#include <os/os_types.h>

struct AddrSpace
{
  paddr_t physAddr;        // physical address of the hardware root page map (page directory)
  SBArray memoryRegions;   // AddrRegion[]
  SBAssocArray addressMap; // virt(addr_t) -> page_t
};

struct Executable
{
  char *path;

  struct MemRegion codeRegion;
  struct MemRegion dataRegion;
  struct MemRegion bssRegion;
};

void initAddrSpace(struct AddrSpace *addrSpace, paddr_t physAddr);
void destroyAddrSpace(struct AddrSpace *addrSpace);
int addAddrSpace(struct AddrSpace *addrSpace);
struct AddrSpace *lookupPhysAddr(paddr_t physAddr);
struct AddrSpace *removeAddrSpace(paddr_t physAddr);
int attachTid(struct AddrSpace *addrSpace, tid_t tid);
int detachTid(tid_t tid);
struct AddrSpace *lookupTid(tid_t tid);
int attachAddrRegion(struct AddrSpace *addrSpace, const struct AddrRegion *addrRegion);
bool findAddress(const struct AddrSpace *addrSpace, addr_t addr);
bool doesOverlap(const struct AddrSpace *addrSpace, const struct MemRegion *region);
int setMapping(struct AddrSpace *addrSpace, addr_t virt, const page_t *page);
int removeMapping(struct AddrSpace *addrSpace, addr_t virt);

#endif /* ADDR_SPACE_H */
