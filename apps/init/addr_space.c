#include "addr_space.h"
#include "phys_mem.h"
#include <os/os_types.h>

struct AddrSpace initsrvAddrSpace;
sbhash_t tidMap;     // tid -> AddrSpace
sbhash_t addrSpaces; // phys addr -> AddrSpace
page_t *pageTable;

/* Initializes an address space and its tables. The physical
   address is the address of the page directory. */

void initAddrSpace(struct AddrSpace *addrSpace, paddr_t physAddr)
{
  addrSpace->physAddr = physAddr;
  sbArrayCreate(&addrSpace->memoryRegions);
  sbHashCreate(&addrSpace->addressMap, 128);
}

void destroyAddrSpace(struct AddrSpace *addrSpace)
{
  sbArrayDelete(&addrSpace->memoryRegions);
  sbHashDestroy(&addrSpace->addressMap);
}

int addAddrSpace(struct AddrSpace *addrSpace)
{
  return (addrSpace
         && sbHashInsert(&addrSpaces, (void *)&addrSpace->physAddr,addrSpace) == 0) ? 0 : -1;
}

struct AddrSpace *lookupPageMap(paddr_t physAddr)
{
  struct AddrSpace *addrSpace;

  if( physAddr == NULL_PADDR )
    return &initsrvAddrSpace;

  return sbHashLookup(&addrSpaces, (void *)&physAddr, (void **)&addrSpace) != 0 ? NULL : addrSpace;
}

int removeAddrSpace(paddr_t physAddr)
{
  return (physAddr == NULL_PADDR
    || sbHashRemove(&addrSpaces, (addr_t *)&physAddr) != 0) ? -1 : 0;
}

/* attachTid() associates a TID to an address space. */

int attachTid(struct AddrSpace *addrSpace, tid_t tid)
{
  if(!addrSpace)
    addrSpace = &initsrvAddrSpace;

  return (tid != NULL_TID
          && sbHashInsert(&tidMap, &tid, addrSpace) == 0) ? 0 : -1;
}

int detachTid(tid_t tid)
{
  return sbHashRemove(&tidMap, &tid) == 0 ? 0 : -1;
}

struct AddrSpace *lookupTid(tid_t tid)
{
  struct AddrSpace *addrSpace;

  return (tid == NULL_TID
         || sbHashLookup(&tidMap, &tid, (void **)&addrSpace) != 0) ? NULL : addrSpace;
}

/* Virtual addresses are allocated to an address space by using
   attachAddrRegion() */

int attachAddrRegion(struct AddrSpace *addrSpace, struct AddrRegion *addrRegion)
{
  struct AddrRegion *tRegion;

  if(!addrSpace)
    addrSpace = &initsrvAddrSpace;

  for(size_t i=0; i < sbArrayCount(&addrSpace->memoryRegions); i++)
  {
    if( sbArrayGet(&addrSpace->memoryRegions, i, (void **)&tRegion) != 0 )
      continue;

    if( regionDoesOverlap(&addrRegion->virtRegion, &tRegion->virtRegion) )
      return -1;
  }

  return sbArrayPush(&addrSpace->memoryRegions, addrRegion) == 0 ? 0 : -1;
}

/* Returns true if a virtual address is allocated in an address space.
   Returns false otherwise. */

bool findAddress(struct AddrSpace *addrSpace, addr_t addr)
{
  return getRegion(addrSpace, addr) != NULL ? true : false;
}

struct AddrRegion *getRegion(struct AddrSpace *addrSpace, addr_t addr)
{
  struct AddrRegion *addr_region;

  if(!addrSpace)
    addrSpace = &initsrvAddrSpace;

  for( size_t i=0; i < sbArrayCount(&addrSpace->memoryRegions); i++ )
  {
    if( sbArrayGet(&addrSpace->memoryRegions, i, (void **)&addr_region) != 0 )
      break;
    else if( regionDoesContain((unsigned int)addr, &addr_region->virtRegion) )
      return addr_region;
  }

  return NULL;
}

/* Checks to see if there's a region that overlaps another region in an address space */

bool doesOverlap(struct AddrSpace *addrSpace, struct MemRegion *region)
{
  struct AddrRegion *addr_region;

  if( !addrSpace )
    addrSpace = &initsrvAddrSpace;

  if( region )
  {
    for( size_t i=0; i < sbArrayCount(&addrSpace->memoryRegions); i++ )
    {
      if( sbArrayGet(&addrSpace->memoryRegions, i, (void **)&addr_region) != 0 )
        break;
      else if( regionDoesOverlap(region, &addr_region->virtRegion) )
        return true;
    }
  }

  return false;
}

// Map a virtual address to a page

int setMapping(struct AddrSpace *addrSpace, addr_t virt, page_t *page)
{
  virt &= ~(PAGE_SIZE-1);

  if(!addrSpace)
    addrSpace = &initsrvAddrSpace;

  return (sbHashInsert(&addrSpace->addressMap, &virt, page) == 0) ? 0 : -1;
}

int getMapping(struct AddrSpace *addrSpace, addr_t virt, page_t **page)
{
  virt &= ~(PAGE_SIZE - 1);

  if(!addrSpace)
    addrSpace = &initsrvAddrSpace;

  return (sbHashLookup(&addrSpace->addressMap, &virt, (void **)page) == 0) ? 0 : -1;
}

// Remove a mapping from a virtual address to a page

int removeMapping(struct AddrSpace *addrSpace, addr_t virt)
{
  virt &= ~(PAGE_SIZE-1);

  if(!addrSpace)
    addrSpace = &initsrvAddrSpace;

  return sbHashRemove(&addrSpace->addressMap, &virt) == 0 ? 0 : -1;
}
