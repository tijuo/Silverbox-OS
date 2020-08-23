#include "addr_space.h"
#include <os/services.h>
#include "phys_mem.h"
#include <os/syscalls.h>
#include "pager.h"

extern void print(const char *);

// XXX: this may also have to record the pages that should be mapped as well (including device pages)

int mapRegion(struct AddrSpace *addrSpace, addr_t virt, paddr_t phys, size_t pages, int flags)
{
  struct AddrRegion region;

  region.virtRegion.start = (int)virt;
  region.virtRegion.length = pages * PAGE_SIZE;
  region.flags = flags;

//  int sysFlags = (region.flags & REG_RO ? PM_READ_ONLY : PM_READ_WRITE);

  if(attachAddrRegion(addrSpace, &region) != 0)
  {
    print("Unable to attach\n");
    return -1;
  }

  if(!findAddress(addrSpace, virt))
    print("Unable to find address that was just attached!\n");

//  else if((flags & MEM_FLG_EAGER) && phys != NULL_PADDR && sys_map(addrSpace->physAddr, virt, (u32)(phys >> 12), pages, sysFlags) != 0)
//    return -1;

  if(phys != NULL_PADDR)
  {
    for(unsigned int i=0; i < pages; i++)
    {
      if(setMapping(addrSpace, virt + i*PAGE_SIZE,
                    &pageTable[(phys+i*PAGE_SIZE) >> 12]) != 0)
      {
        print("Unable to map\n");
        return -1;
      }
    }
  }

//  print("Mapping region virt: 0x"), printHex(virt), print(" phys: 0x"), printHex((addr_t)phys), print(" "), printInt(pages), print(" pages. flags: "), printInt(flags), print("\n");
  return 0;
}
