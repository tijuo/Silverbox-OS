#include "addr_space.h"
#include <os/services.h>
#include "phys_mem.h"
#include <os/syscalls.h>
#include "pager.h"

// XXX: this may also have to record the pages that should be mapped as well (including device pages)

int mapRegion(struct AddrSpace *addrSpace, addr_t virt, paddr_t phys, size_t pages, int flags)
{
  struct AddrRegion region;

  region.virtRegion.start = (int)virt;
  region.virtRegion.length = pages * PAGE_SIZE;
  region.flags = ((flags & MEM_FLG_RO) ? REG_RO : 0) | ((flags & MEM_FLG_COW) ? REG_RO | REG_COW : 0);

  int sysFlags = (region.flags & REG_RO ? PM_READ_ONLY : PM_READ_WRITE);

  if(attachAddrRegion(addrSpace, &region) != 0)
    return -1;
  else if((flags & MEM_FLG_EAGER) && phys != NULL_PADDR)
    return sys_map(addrSpace->physAddr, virt, (u32)(phys >> 12), sysFlags, pages);

  return 0;
}
