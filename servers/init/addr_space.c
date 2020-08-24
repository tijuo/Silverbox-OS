#include "addr_space.h"
#include "phys_mem.h"
#include <os/os_types.h>

struct AddrSpace initsrvAddrSpace;
sbhash_t addrSpaces; // phys addr/tid -> AddrSpace
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
  char *a = malloc(19);

  if(!a)
    return -1;

  a[0] = '0';
  a[1] = 'x';

  itoa(addrSpace->physAddr, &a[2], 16);

  return (addrSpace && sbHashInsert(&addrSpaces, a, addrSpace) == 0) ? 0 : -1;
}

struct AddrSpace *lookupPageMap(paddr_t physAddr)
{
  struct AddrSpace *addrSpace;
  char a[19] = { '0', 'x' };

  if( physAddr == NULL_PADDR )
    return &initsrvAddrSpace;

  itoa(physAddr, &a[2], 16);

  return sbHashLookup(&addrSpaces, a, (void **)&addrSpace) != 0 ? NULL : addrSpace;
}

int removeAddrSpace(paddr_t physAddr)
{
  struct AddrSpace *addrSpace;

  char a[19] = { '0', 'x' };
  char *storedKey;

  itoa(physAddr, &a[2], 16);

  if(physAddr != NULL_PADDR && sbHashRemovePair(&addrSpaces, a, &storedKey, (void **)&addrSpace) == 0)
  {
    free(storedKey);
    return 0;
  }
  else
    return -1;
}

/* attachTid() associates a TID to an address space. */

int attachTid(struct AddrSpace *addrSpace, tid_t tid)
{
  char *a = malloc(6);

  if(!a)
    return -1;

  if(!addrSpace)
    addrSpace = &initsrvAddrSpace;

  itoa(tid, a, 10);

  return (tid != NULL_TID && sbHashInsert(&addrSpaces, a, addrSpace) == 0) ? 0 : -1;
}

int detachTid(tid_t tid)
{
  struct AddrSpace *addrSpace;
  char *storedKey;
  char a[6];

  itoa(tid, a, 10);

  if(sbHashRemovePair(&addrSpaces, a, &storedKey, (void **)&addrSpace) == 0)
  {
    free(storedKey);
    return 0;
  }
  else
    return -1;
}

struct AddrSpace *lookupTid(tid_t tid)
{
  struct AddrSpace *addrSpace;
  char a[6];

  itoa(tid, a, 10);

  return (tid == NULL_TID
         || sbHashLookup(&addrSpaces, a, (void **)&addrSpace) != 0) ? NULL : addrSpace;
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

  return (sbArrayPush(&addrSpace->memoryRegions, addrRegion)) == 0 ? 0 :  -1;
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
  char *a = malloc(10);

  if(!a)
    return -1;

  a[0] = '0';
  a[1] = 'x';

  virt &= ~(PAGE_SIZE-1);

  if(!addrSpace)
    addrSpace = &initsrvAddrSpace;

  itoa((int)virt, &a[2], 16);

  return (sbHashInsert(&addrSpace->addressMap, a, page) == 0) ? 0 : -1;
}

int getMapping(struct AddrSpace *addrSpace, addr_t virt, page_t **page)
{
  char a[10] = { '0', 'x' };

  virt &= ~(PAGE_SIZE - 1);

  if(!addrSpace)
    addrSpace = &initsrvAddrSpace;

  itoa((int)virt, &a[2], 16);

  return (sbHashLookup(&addrSpace->addressMap, a, (void **)page) == 0) ? 0 : -1;
}

// Remove a mapping from a virtual address to a page

int removeMapping(struct AddrSpace *addrSpace, addr_t virt)
{
  char *storedKey;
  char a[10] = { '0', 'x' };

  virt &= ~(PAGE_SIZE-1);

  if(!addrSpace)
    addrSpace = &initsrvAddrSpace;

  itoa((int)virt, &a[2], 16);

  // XXX: Must also free() the key

  if(sbHashRemovePair(&addrSpace->addressMap, a, &storedKey, NULL) == 0)
  {
    free(storedKey);
    return 0;
  }
  else
    return -1;
}
