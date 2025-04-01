#include "addr_space.h"
#include "phys_mem.h"
#include <stdio.h>

AddrSpace initsrv_addr_space;
IntHashTable addr_spaces; // phys addr/tid -> AddrSpace

int tid_cmp(tid_t *x, tid_t *y) {
    return *x == *y ? 0 : 1;
}

/* Initializes an address space and its tables. The physical
   address is the address of the page directory. */
int addr_space_init(AddrSpace* addr_space, paddr_t phys_addr)
{
    addr_space->phys_addr = phys_addr;
    
    if(dyn_array_init(&addr_space->mappings, sizeof(MemoryMapping), 17, &GLOBAL_ALLOCATOR) != 0) {
        return -1;
    }

    if(dyn_array_init(&addr_space->tids, sizeof(tid_t), 17, &GLOBAL_ALLOCATOR) != 0) {
        dyn_array_destroy(&addr_space->mappings);
        return -1;
    }

    addr_space->exe_fd = -1;

    return 0;
}

void addr_space_destroy(AddrSpace* addr_space)
{
    dyn_array_destroy(&addr_space->tids);
    dyn_array_destroy(&addr_space->mappings);
}

int addr_space_add(AddrSpace* addr_space)
{
    return int_hash_table_insert(&addr_spaces, (int)addr_space->phys_addr, addr_space, sizeof *addr_space);
}

AddrSpace* lookup_page_map(paddr_t phys_addr)
{
    AddrSpace* addr_space;

    if(phys_addr == CURRENT_ROOT_PMAP) {
        return &initsrv_addr_space;
    } else if(int_hash_table_lookup(&addr_spaces, (int)phys_addr, &addr_space, NULL) != 0) {
        return addr_space;
    } else {
        return NULL;
    }
}

int remove_addr_space(paddr_t phys_addr)
{
    return int_hash_table_remove(&addr_spaces, (int)phys_addr);
}

/* attach_tid() associates a TID with an address space. */
int attach_tid(AddrSpace* addr_space, tid_t tid)
{
    if(!addr_space) {
        addr_space = &initsrv_addr_space;
    }

    return (tid != NULL_TID && dyn_array_push(&addr_space->tids, &tid)) ? 0 : -1;
}

int detach_tid(AddrSpace *addr_space, tid_t tid)
{
    if(!addr_space) {
        addr_space = &initsrv_addr_space;
    }

    int ndx = dyn_array_find(addr_space, &tid, tid_cmp);

    if(ndx < 0) {
        return -1;
    }

    return (dyn_array_remove_unstable(&addr_space->tids, ndx) == 0) ? 0 : -1;
}

/*
AddrSpace* lookup_tid(tid_t tid)
{
    AddrSpace* addr_space;
    char a[6];

    itoa(tid, a, 10);

    return (tid == NULL_TID
        || int_hash_table_lookup(&addr_spaces, a, (void**)&addr_space, NULL) != 0) ? NULL : addr_space;
}
*/

/* Virtual addresses are allocated to an address space by using
   attach_addr_region() */

int attach_addr_region(AddrSpace* addr_space, MemoryMapping *mapping)
{
    MemoryMapping t_map1;
    MemoryMapping t_map2;

    if(!addr_space) {
        addr_space = &initsrv_addr_space;
    }

    if(dyn_array_length(&addr_space->mappings) == 0) {
        return (dyn_array_push(&addr_space->mappings, mapping)) == 0 ? 0 : -1;
    }
    
    dyn_array_get(&addr_space->mappings, 0, &t_map1);
    dyn_array_get(&addr_space->mappings, -1, &t_map2);

    if(mapping->memory_region.end <= t_map1.memory_region.start) {
        return (dyn_array_insert(&addr_space->mappings, 0, mapping)) == 0 ? 0 : -1;
    } else if(mapping->memory_region.start >= t_map2.memory_region.end) {
        return (dyn_array_push(&addr_space->mappings, mapping)) == 0 ? 0 : -1;
    } else {
        for(size_t i = 1; i < dyn_array_length(&addr_space->mappings); i++, t_map1 = t_map2) {
            dyn_array_get(&addr_space->mappings, i, &t_map2);

            if(mapping->memory_region.start >= t_map1.memory_region.end && mapping->memory_region.end <= t_map2.memory_region.start) {
                return (dyn_array_insert(&addr_space->mappings, i, mapping)) == 0 ? 0 : -1;
            }
        }
    }

    return -1;
}

/* Returns true if a virtual address is allocated in an address space.
   Returns false otherwise. */

bool find_address(const AddrSpace* addr_space, addr_t addr)
{
    return get_region(addr_space, addr) != NULL;
}

MemoryMapping* get_mapping(const AddrSpace* addr_space, addr_t addr)
{
    MemoryMapping *mapping;

    if(!addr_space) {
        addr_space = &initsrv_addr_space;
    }

    for(size_t i = 0; i < dyn_array_length(&addr_space->mappings); i++) {
        if(dyn_array_get(&addr_space->mappings, i, &mapping) != 0) {
            break;
        }

        if(region_contains(&mapping->memory_region, addr)) {
            return mapping;
        }
    }

    return NULL;
}

// Remove a section of a mapped region from an address space

// Note: This does not unmap any pages associated with the region

int remove_mapping(AddrSpace* addr_space, MemRegion *region)
{
    MemoryMapping *mapping;
    MemRegion differences[2];

    if(!addr_space) {
        addr_space = &initsrv_addr_space;
    }

    for(size_t i = 0; i < dyn_array_length(&addr_space->mappings); i++) {
        if(dyn_array_get(&addr_space->mappings, i, &mapping) != 0) {
            break;
        }

        if(region_intersects(&mapping->memory_region, region)) {
            int count = region_difference(&mapping->memory_region, region, &differences);

            if(count == 0) {    // Region to be removed is at least as large as an entire mapped region
                dyn_array_remove(&addr_space->mappings, i);
            } else if(count == 1) {
                if(region_equals(&differences[0], mapping)) {       // The region to be removed is an entire `MemoryMapping`
                    dyn_array_remove(&addr_space->mappings, i);
                } else {                                            // Only remove a portion of the mapping
                    mapping->file_offset += differences[0].start - mapping->memory_region.start;
                    mapping->memory_region = differences[0];
                }
            } else if(count == 2) {     // The region to be removed between an existing region which would create a hole.
                MemoryMapping new_mapping = *mapping;

                new_mapping.memory_region.start = differences[1].start;
                new_mapping.file_offset += differences[1].start - mapping->memory_region.start;

                if(dyn_array_insert(&addr_space->mappings, i + 1, &new_mapping) != 0) {
                    return -1;
                }

                mapping->memory_region.end = differences[0].end;
            }
        }
    }

    return 0;
}
