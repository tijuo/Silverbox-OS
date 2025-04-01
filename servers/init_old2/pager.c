#include "addr_space.h"
#include <os/services.h>
#include "phys_mem.h"
#include <os/syscalls.h>
#include "pager.h"
#include <stdio.h>

// XXX: this may also have to record the pages that should be mapped as well (including device pages)

int region_map(AddrSpace* addr_space, addr_t virt, paddr_t phys, size_t pages, int flags)
{
    struct AddrRegion* region = malloc(sizeof(struct AddrRegion));

    if(!region)
        return -1;

    region->virt_region.start = (int)virt;
    region->virt_region.length = pages * PAGE_SIZE;
    region->flags = flags;

    //  int sysFlags = (region.flags & REG_RO ? PM_READ_ONLY : PM_READ_WRITE);

      // XXX: region will need to be unmapped

    if(attach_addr_region(addr_space, region) != 0) {
        fprintf(stderr, "Unable to attach\n");
        return -1;
    }

    // pbase_t frame = (pbase_t)(phys >> 12);

    if(!find_address(addr_space, virt)) {
        fprintf(stderr, "Unable to find address that was just attached!\n");
    }

    //  else if((flags & MEM_FLG_EAGER) && phys != NULL_PADDR && sys_map(addr_space->phys_addr, virt, &frame, pages, sysFlags) != (int)pages)
    //    return -1;

    if(phys != CURRENT_ROOT_PMAP) {
        for(unsigned int i = 0; i < pages; i++) {
            if(set_mapping(addr_space, virt + i * PAGE_SIZE,
                &page_table[(phys + i * PAGE_SIZE) >> 12]) != 0) {
                fprintf(stderr, "Unable to map\n");
                return -1;
            }
        }
    }

    //  fprintf(stderr, "Mapping region virt: %#08x -> phys: %#08x %d pages. flags: %#x\n", virt, phys, pages, flags);
    return 0;
}
