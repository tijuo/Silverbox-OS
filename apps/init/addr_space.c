#include "addr_space.h"
#include "phys_mem.h"
#include <os/os_types.h>

struct AddrSpace initsrvAddrSpace;
SBAssocArray tidMap;     // tid -> AddrSpace
SBAssocArray addrSpaces; // phys addr -> AddrSpace

/* Initializes an address space and its tables. The physical
   address is the address of the page directory. */

void initAddrSpace(struct AddrSpace *addrSpace, paddr_t physAddr)
{
  addrSpace->physAddr = physAddr;
  sbArrayCreate(&addrSpace->memoryRegions);
  sbAssocArrayCreate(&addrSpace->addressMap, 2);
}

void destroyAddrSpace(struct AddrSpace *addrSpace)
{
  sbArrayDelete(&addrSpace->memoryRegions);
  sbAssocArrayDelete(&addrSpace->addressMap);
}

int addAddrSpace(struct AddrSpace *addrSpace)
{
  if( !addrSpace )
    return -1;

  return sbAssocArrayInsert(&addrSpaces, (void *)&addrSpace->physAddr,
        sizeof addrSpace->physAddr, addrSpace, sizeof *addrSpace) == 0 ? 0 : -1;
}

struct AddrSpace *lookupPhysAddr(paddr_t physAddr)
{
  struct AddrSpace *addrSpace;

  if( physAddr == NULL_PADDR )
    return &initsrvAddrSpace;

  return sbAssocArrayLookup(&addrSpaces, (void *)&physAddr, sizeof(void *),
        (void **)&addrSpace, NULL) < 0 ? NULL : addrSpace;
}

struct AddrSpace *removeAddrSpace(paddr_t physAddr)
{
  struct AddrSpace *addrSpace;

  if( physAddr == NULL_PADDR )
    return NULL;

  return sbAssocArrayRemove(&addrSpaces, (addr_t *)&physAddr, sizeof physAddr,
        (void **)&addrSpace, NULL) < 0 ? NULL : addrSpace;
}

/* attachTid() associates a TID to an address space. */

int attachTid(struct AddrSpace *addrSpace, tid_t tid)
{
  if(!addrSpace || tid == NULL_TID )
    return -1;

  return sbAssocArrayInsert(&tidMap, &tid, sizeof tid, addrSpace, sizeof *addrSpace) == 0 ? 0 : -1;
}

int detachTid(tid_t tid)
{
  return sbAssocArrayRemove(&tidMap, &tid, sizeof tid, NULL, NULL) == 0 ? 0 : -1;
}

struct AddrSpace *lookupTid(tid_t tid)
{
  struct AddrSpace *addrSpace;

  if(tid == NULL_TID)
    return NULL;

  return sbAssocArrayLookup(&tidMap, &tid, sizeof tid,
                            (void **)&addrSpace, NULL) != 0 ? NULL : addrSpace;
}

/* Virtual addresses are allocated to an address space by using
   attachAddrRegion() */

int attachAddrRegion(struct AddrSpace *addrSpace, const struct AddrRegion *addrRegion)
{
  struct AddrRegion *tRegion;

  if(!addrSpace)
    addrSpace = &initsrvAddrSpace;

  for(int i=0; i < sbArrayCount(&addrSpace->memoryRegions); i++)
  {
    if( sbArrayElemAt(&addrSpace->memoryRegions, i, (void **)&tRegion, NULL) != 0 )
      continue;

    if( regionDoesOverlap(&addrRegion->virtRegion, &tRegion->virtRegion) )
      return -1;
  }

  return sbArrayPush(&addrSpace->memoryRegions, addrRegion, sizeof *addrRegion) == 0 ? 0 : -1;
}

/* Returns true if a virtual address is allocated in an address space.
   Returns false otherwise. */

bool findAddress(const struct AddrSpace *addrSpace, addr_t addr)
{
  struct AddrRegion *addr_region;

  if(!addrSpace)
    addrSpace = &initsrvAddrSpace;

  for( int i=0; i < sbArrayCount(&addrSpace->memoryRegions); i++ )
  {
    if( sbArrayElemAt(&addrSpace->memoryRegions, i, (void **)&addr_region, NULL) != 0 )
      return false;

    if( regionDoesContain((unsigned int)addr, &addr_region->virtRegion) )
      return true;
  }

  return false;
}

/* Checks to see if there's a region that overlaps another region in an address space */

bool doesOverlap(const struct AddrSpace *addrSpace, const struct MemRegion *region)
{
  const struct AddrRegion *addr_region;

  if( !addrSpace )
    addrSpace = &initsrvAddrSpace;

  if( !region )
    return false;

  for( int i=0; i < sbArrayCount(&addrSpace->memoryRegions); i++ )
  {
    if( sbArrayElemAt(&addrSpace->memoryRegions, i, (void **)&addr_region, NULL) != 0 )
      return false;

    if( regionDoesOverlap(region, &addr_region->virtRegion) )
      return true;
  }

  return false;
}

// Map a virtual address to a page

int setMapping(struct AddrSpace *addrSpace, addr_t virt, const page_t *page)
{
  virt &= ~(PAGE_SIZE-1);

  if(!addrSpace)
    addrSpace = &initsrvAddrSpace;

  addr_t *key = malloc(sizeof(addr_t));

  if(!key)
    return -1;

  *key = virt;

  return (sbAssocArrayInsert(&addrSpace->addressMap, key, sizeof *key,
                        page, sizeof *page) == 0) ? 0 : -1;
}

// Remove a mapping from a virtual address to a page

int removeMapping(struct AddrSpace *addrSpace, addr_t virt)
{
  virt &= ~(PAGE_SIZE-1);

  if(!addrSpace)
    addrSpace = &initsrvAddrSpace;

  page_t *page;

  return sbAssocArrayRemove(&addrSpace->addressMap, &virt, sizeof virt, (void *)&page, NULL) == 0 ? 0 : -1;
}
