#include <kernel/mm.h>
#include <kernel/debug.h>
#include <kernel/thread.h>
#include <kernel/memory.h>
#include <kernel/pae.h>
#include <kernel/error.h>
#include <oslib.h>
#include <string.h>
#include <kernel/bits.h>

paddr_t max_phys_addr = (1ul << MAX_PHYS_ADDR_BITS);
int is_1gb_pages_supported;
unsigned int num_paging_levels = 4;

ALIGNED(PAGE_SIZE) pmap_entry_t kmap_area_ptab[PAE_PMAP_ENTRIES];

NON_NULL_PARAMS
static int access_mem(addr_t address, size_t len, void *buffer, paddr_t root_pmap,
bool read);

/**
 Zero out a page frame.

 @param phys Physical address frame to clear.
 @return E_OK on success. E_FAIL on failure.
 */

int clear_phys_page(paddr_t phys) {
  long int dummy;
  long int dummy2;

  __asm__("rep stosq"
	  : "=c"(dummy2), "=D"(dummy)
		: "a"(0), "c"(512), "D"(phys));

  return E_OK;
}

/*
int map_large_frame(uint64_t phys, pmap_entry_t *pmap_entry) {
  if(phys >= MAX_PHYS_MEMORY)
    RET_MSG(EFAIL, "Error: Physical frame is out of range.");

  pde_t old_pde = read_pde(PDE_INDEX(KMAP_AREA), CURRENT_ROOT_PMAP);
  pmap_entry_t mapped_pde = {
    .value = 0
  };

  set_large_pde_base(&mapped_pde.large_pde, PADDR_TO_PFRAME(phys));
  mapped_pde.large_pde.is_large_page = 1;
  mapped_pde.large_pde.is_read_write = 1;
  mapped_pde.large_pde.is_present = 1;

  if(IS_ERROR(
      write_pde(PDE_INDEX(KMAP_AREA), mapped_pde.pde, CURRENT_ROOT_PMAP)))
    RET_MSG(E_FAIL, "Error: Unable to write PDE.");

  invalidate_page(KMAP_AREA);

  if(pmap_entry)
    pmap_entry->pde = old_pde;

  return E_OK;
}

int unmap_large_frame(pmap_entry_t pmap_entry) {
  if(IS_ERROR(
      write_pde(PDE_INDEX(KMAP_AREA), pmap_entry.pde, CURRENT_ROOT_PMAP)))
    RET_MSG(E_FAIL, "Error: Unable to write PDE.");

  invalidate_page(KMAP_AREA);
  return E_OK;
}
*/

/**
 Set up a new page map to be used as an address space for a thread. It mainly
 inserts the kernel page mappings.

 @param root_pmap The address of the page map
 @return E_OK, on success. E_FAIL, on error.
 */

int initialize_root_pmap(paddr_t root_pmap) {
  pmap_entry_t *pml4_table = (pmap_entry_t *)root_pmap;
  volatile pmap_entry_t *current_pml4_table = (volatile pmap_entry_t *)get_root_page_map();

  pml4_table[0] = current_pml4_table[0];

  return E_OK;
}

int translate_vaddr(addr_t vaddr, paddr_t *paddr, paddr_t root_pmap) {
  _Static_assert(num_paging_levels > 2, "num_paging_levels must be greater than 2");
  unsigned int paging_level = num_paging_levels-1;
  volatile pmap_entry_t *table = (volatile pmap_entry_t *)root_pmap;

  while(--paging_level) {
	pmap_entry_t entry = table[(vaddr >> (PAGE_BITS + PAE_ENTRY_BITS*(paging_level)))
							   % PAE_PMAP_ENTRIES];

	if(!IS_FLAG_SET(entry, PAE_PRESENT))
	  return E_FAIL;

	if(IS_FLAG_SET(entry, PAE_PS))
	  break;
	else {
	  switch(paging_level) {
		case 2:
		  table = (volatile pmap_entry_t *)get_pdpte_base(entry);
		  break;
		case 1:
		  table = (volatile pmap_entry_t *)get_pde_base(entry);
		  break;
		case 0:
		  table = (volatile pmap_entry_t *)get_pte_base(entry);
		  break;
		default:
		  return E_FAIL;
	  }
	}
  }

  return E_OK;
}

/**
 * Is a particular address in an address space accessible by the user?
 *
 * @param addr The virtual address.
 * @param root_pmap The physical address of the root page map table.
 * @param is_read Is the access a read?
 *
 * @return true if the address can be accessed. false, otherwise.
 */

/*
bool is_user_accessible(addr_t addr, paddr_t root_pmap, bool is_read) {
  _Static_assert(num_paging_levels > 2, "num_paging_levels must be greater than 2");
  unsigned int paging_level = num_paging_levels-1;
  volatile pmap_entry_t *table = (volatile pmap_entry_t *)root_pmap;

  while(--paging_level) {
	pmap_entry_t entry = table[(addr >> (PAGE_BITS + PAE_ENTRY_BITS*(paging_level)))
							   % PAE_PMAP_ENTRIES];

	if(!IS_FLAG_SET(entry, PAE_US) || !IS_FLAG_SET(entry, PAE_PRESENT)
		|| (!is_read && !IS_FLAG_SET(entry, PAE_RW))) {
	  return false;
	}

	if(IS_FLAG_SET(entry, PAE_PS))
	  break;
	else {
	  switch(paging_level) {
		case 2:
		  table = (volatile pmap_entry_t *)get_pdpte_base(entry);
		  break;
		case 1:
		  table = (volatile pmap_entry_t *)get_pde_base(entry);
		  break;
		case 0:
		  table = (volatile pmap_entry_t *)get_pte_base(entry);
		  break;
		default:
		  return false;
	  }
	}
  }

  return true;
}
*/

/**
 Allows for reading/writing a block of memory according to the address space

 This function is used to read/write data to/from any address space. If reading,
 read len bytes from address into buffer. If writing, write len bytes from
 buffer to address.

 This assumes that the memory regions don't overlap.

 @param address The address in the address space to perform the read/write.
 @param len The length of the block to read/write.
 @param buffer The buffer in the current address space that is used for the read/write.
 @param root_pmap The physical address of the address space.
 @param read True if reading. False if writing.
 @return E_OK on success. E_FAIL on failure.
 */

NON_NULL_PARAMS static int access_mem(addr_t address, size_t len, void *buffer,
                                     paddr_t root_pmap,
                                     bool read)
{
  size_t buffer_offset = 0;

  assert(address);

  while(len) {
    paddr_t base_address;
    size_t addr_offset = address % PAGE_SIZE;
    size_t bytes =
        (len > PAGE_SIZE - addr_offset) ? PAGE_SIZE - addr_offset : len;

    if(IS_ERROR(translate_vaddr(address, &base_address, root_pmap)))
    	return E_FAIL;

    if(read)
      memcpy((void *)((addr_t)buffer + buffer_offset), base_address + addr_offset, bytes);
    else
      memcpy(base_address + addr_offset, (void *)((addr_t)buffer + buffer_offset), bytes);

    address += bytes;
    buffer_offset += bytes;
    len -= bytes;
  }

  return E_OK;
}

/**
 Write data to an address in an address space.

 Equivalent to an access_mem( address, len, buffer, root_pmap, false ).

 @param address The address in the address space to perform the write.
 @param len The length of the block to write.
 @param buffer The buffer in the current address space that is used for the write.
 @param root_pmap The physical address of the address space.
 @return E_OK on success. E_FAIL on failure.
 */

NON_NULL_PARAMS int poke_virt(addr_t address, size_t len, void *buffer,
                             paddr_t root_pmap)
{
  if(root_pmap == get_root_page_map() || root_pmap == CURRENT_ROOT_PMAP) {
    memcpy((void*)address, buffer, len);
    return E_OK;
  }
  else
    return access_mem(address, len, buffer, root_pmap, false);
}

/* data in 'address' goes to 'buffer' */

/**
 Read data from an address in an address space.

 Equivalent to an access_mem( address, len, buffer, root_pmap, true ).

 @param address The address in the address space to perform the read.
 @param len The length of the block to read.
 @param buffer The buffer to store the data read from address.
 @param root_pmap The physical address of the root page map.
 @return E_OK on success. E_FAIL on failure.
 */

NON_NULL_PARAMS int peek_virt(addr_t address, size_t len, void *buffer,
                             paddr_t root_pmap)
{
  if(root_pmap == get_root_page_map() || root_pmap == CURRENT_ROOT_PMAP) {
    memcpy(buffer, (void*)address, len);
    return E_OK;
  }
  else
    return access_mem(address, len, buffer, root_pmap, true);
}
