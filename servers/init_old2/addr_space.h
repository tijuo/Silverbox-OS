#ifndef ADDR_SPACE_H
#define ADDR_SPACE_H

#include <os/region.h>
#include <stddef.h>
#include <oslib.h>

#define REG_RO		    1	// Read-only memory range
#define REG_COW		    2	/* Range is copy-on-write (if written to, copy 
                   range to another set of pages & mark that
                   range as read-write [implies read-only]). */
#define REG_LAZY	    4	// Do not immediately map the range into memory
#define REG_PIN         8   // Do not swap out the memory within this region.
#define REG_NOEXEC	    32	// Do not allow code execution on memory range
#define REG_DOWN	    64	// Grow region down instead of up (for stacks)
#define REG_GUARD	    128

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
  Address region are areas of the address space that can be marked with various flags. Upon
  a page fault, the memory regions are checked for their flags and be mapped appropriately,
  if no protection fault has occurred.

*/

typedef struct {
    int fd;
    size_t file_offset;
    int permissions;
    int flags;
    MemRegion memory_region;
} MemoryMapping;

#include <os/ostypes/dynarray.h>
#include <os/ostypes/hashtable.h>

typedef struct {
    paddr_t phys_addr;              // physical address of the hardware root page map (page directory)
    DynArray mappings;              // DynArray(MemoryMapping) XXX: Consider using another more efficient data structure (if any)
    DynArray tids;                  // DynArray(tid_t) XXX: Consider using another more efficient data structure (if any)
    int exe_fd;                     // Executable file descriptor
} AddrSpace;

// XXX: Replace this with the ELF program header

int addr_space_init(AddrSpace* addr_space, paddr_t phys_addr);
void addr_space_destroy(AddrSpace* addr_space);
int addr_space_add(AddrSpace* addr_space);
AddrSpace* lookup_page_map(paddr_t phys_addr);
int remove_addr_space(paddr_t phys_addr);
int attach_tid(AddrSpace* addr_space, tid_t tid);
int detach_tid(AddrSpace *addr_space, tid_t tid);
AddrSpace* lookup_tid(tid_t tid);
int attach_addr_region(AddrSpace* addr_space, MemoryMapping *mapping);
bool find_address(const AddrSpace* addr_space, addr_t addr);
int remove_mapping(AddrSpace* addr_space, MemRegion *region);
MemoryMapping* get_mapping(const AddrSpace* addr_space, addr_t addr);

extern AddrSpace initsrv_addr_space;
extern StringHashTable addr_spaces; // phys addr -> AddrSpace

#endif /* ADDR_SPACE_H */
