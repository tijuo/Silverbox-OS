#include "init.h"
#include "memory.h"
#include <kernel/multiboot2.h>
#include <kernel/memory.h>

DISC_DATA addr_t first_free_page;

DISC_CODE addr_t alloc_page_frame(void);

DISC_CODE bool is_reserved_page(uint64_t addr, multiboot_info_t *info,
								int is_large_page);

DISC_CODE int init_memory(multiboot_info_t *info);
DISC_CODE static void setup_gdt(void);

DISC_DATA ALIGNED(PAGE_SIZE) pmap_entry_t kpage_dir[PMAP_ENTRY_COUNT]; // The initial page directory used by the kernel on bootstrap

extern pte_t kmap_area_ptab[PTE_ENTRY_COUNT];
extern volatile gdt_entry_t kernel_gdt[6];
extern multiboot_info_t *multiboot_info;

addr_t alloc_page_frame(void) {
  addr_t frame = first_free_page;

  while(frame >= EXTENDED_MEMORY && is_reserved_page(frame, multiboot_info, 0))
  {
	frame += PAGE_SIZE;
  }

  if(frame < EXTENDED_MEMORY)
	return INVALID_PFRAME;

  first_free_page = frame + PAGE_SIZE;

  return frame;
}

bool is_reserved_page(uint64_t addr, multiboot_info_t *info, int is_large_page)
{
  unsigned int kernel_start = (unsigned int)&kphys_start;
  unsigned int kernel_length = (unsigned int)ksize;
  uint64_t addr_end;

  if(is_large_page) {
	addr = addr & ~(PAGE_TABLE_SIZE - 1);
	addr_end = addr + PAGE_TABLE_SIZE;
  }
  else {
	addr = addr & ~(PAGE_SIZE - 1);
	addr_end = addr + PAGE_SIZE;
  }

  if(addr < (uint64_t)EXTENDED_MEMORY)
	return true;
  else if((addr >= kernel_start && addr < kernel_start + kernel_length)
	  || (addr_end >= kernel_start && addr_end < kernel_start + kernel_length))
	return true;
  else if(addr >= (addr_t)EXT_PTR(ktcb_start)
	  && addr < (addr_t)EXT_PTR(ktcb_end))
	return true;
  else {
	int in_some_region = 0;
	const memory_map_t *mmap;

	for(unsigned int offset = 0; offset < info->mmap_length;
		offset += mmap->size + sizeof(mmap->size))
	{
	  mmap = (const memory_map_t*)KPHYS_TO_VIRT(info->mmap_addr + offset);

	  uint64_t mmap_len = mmap->length_high;
	  mmap_len = mmap_len << 32;
	  mmap_len |= mmap->length_low;

	  uint64_t mmap_base = mmap->base_addr_high;
	  mmap_base = mmap_base << 32;
	  mmap_base |= mmap->base_addr_low;

	  if(((uint64_t)addr >= mmap_base && (uint64_t)addr <= mmap_base + mmap_len) || ((uint64_t)addr_end
		  >= mmap_base
																					 && (uint64_t)addr_end <= mmap_base
																						 + mmap_len))
	  {
		in_some_region = 1;

		if(mmap->type != MBI_TYPE_AVAIL)
		  return true;
		else
		  break;
	  }
	}

	if(!in_some_region)
	  return true;

	module_t *module = (module_t*)KPHYS_TO_VIRT(info->mods_addr);

	for(unsigned int i = 0; i < info->mods_count; i++, module++) {
	  if(addr > module->mod_start && addr >= module->mod_end
		 && addr_end > module->mod_start && addr_end > module->mod_end)
		continue;
	  else if(addr < module->mod_start && addr < module->mod_end
			  && addr_end <= module->mod_start && addr_end < module->mod_end)
		continue;
	  else
		return true;
	}

	return false;
  }
}

void setup_gdt(void) {
  volatile gdt_entry_t *tss_desc = &kernel_gdt[TSS_SEL / sizeof(gdt_entry_t)];
  struct PseudoDescriptor gdt_pointer = {
	.base = (uint64_t)kernel_gdt,
	.limit = 6 * sizeof(gdt_entry_t)
  };

  size_t tss_limit = sizeof tss;
  tss_desc->base1 = (uint16_t)((uintptr_t)&tss & 0xFFFFul);
  tss_desc->base2 = (uint8_t)(((uintptr_t)&tss >> 16) & 0xFFul);
  tss_desc->base3 = (uint8_t)(((uintptr_t)&tss >> 24) & 0xFFul);
  tss_desc->access_flags = GDT_SYS | GDT_TSS | GDT_DPL0 | GDT_PRESENT;
  tss_desc->flags2 = GDT_BYTE_GRAN;
  tss_desc->limit1 = (uint16_t)(tss_limit & 0xFFFFu); // Size of TSS structure and IO Bitmap (in pages)
  tss_desc->limit2 = (uint8_t)(tss_limit >> 16);

  // Set the single kernel stack that will be shared between threads

  tss.rsp0 = (uint64_t)kernel_stack_top;

  __asm__ __volatile__("lgdt %0" :: "m"(gdt_pointer) : "memory");
  __asm__ __volatile__("ltr %%ax" :: "a"(TSS_SEL));
}

int init_memory(multiboot_info_t *info) {
  first_free_page = KVIRT_TO_PHYS((addr_t )&ktcb_end);

  unsigned int total_phys_mem = (info->mem_upper + 1024) * 1024;

  if(IS_FLAG_SET(info->flags, MBI_FLAGS_MEM))
	kprintf("Lower Memory: %d bytes Upper Memory: %d bytes\n",
			info->mem_lower << 10, info->mem_upper << 10);
  kprintf("Total Memory: %#x. %d pages.\n", total_phys_mem,
		  total_phys_mem / PAGE_SIZE);

  if(total_phys_mem < (64 << 20)) {
	kprintf("Not enough memory. System must have at least 64 MiB of memory.\n");
	return E_FAIL;
  }

  kprintf("Kernel AddrSpace: %#p\n", kpage_dir);

#ifdef DEBUG

  kprintf("TCB Table size: %d bytes\n", ktcb_table_size);
#endif /* DEBUG */

  return E_OK;
}
