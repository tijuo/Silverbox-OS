#include "init.h"
#include "memory.h"
#include <kernel/multiboot2.h>
#include <kernel/pae.h>
#include <kernel/memory.h>
#include <util.h>

#define MIN_MEM_MB_REQ			64

DISC_DATA addr_t first_free_page;

DISC_CODE addr_t alloc_page_frame(void);

DISC_CODE bool is_reserved_page(uint64_t addr, const struct multiboot_info_header *header);

DISC_CODE int init_memory(const struct multiboot_info_header *header);
DISC_CODE static void setup_gdt(void);

DISC_DATA ALIGNED(PAGE_SIZE) pmap_entry_t kpage_dir[PAE_PMAP_ENTRIES]; // The initial page directory used by the kernel on bootstrap

extern volatile gdt_entry_t kernel_gdt[6];

const struct multiboot_info_header *mb_info_header;

addr_t alloc_page_frame(void) {
  addr_t frame = first_free_page;

  while(frame >= EXTENDED_MEMORY && is_reserved_page(frame, mb_info_header)) {
	frame += PAGE_SIZE;
  }

  if(frame < EXTENDED_MEMORY)
    return INVALID_PADDR;

  first_free_page = frame + PAGE_SIZE;

  return frame;
}

NON_NULL_PARAMS bool is_reserved_page(uint64_t addr,
  const struct multiboot_info_header *header)
{
  uintptr_t kernel_start = (uintptr_t)&kphys_start;
  unsigned long int kernel_length = (unsigned long int)ksize;
  uint64_t addr_end;
  addr = addr & ~(PAGE_SIZE - 1);
  addr_end = addr + PAGE_SIZE;

  const struct multiboot_tag *tags = header->tags;

  if(addr < (uint64_t)EXTENDED_MEMORY) {
	kprintf("%#lx-%#lx overlaps conventional memory.\n", addr, addr_end);
	return true;
  }
  else if((addr >= kernel_start && addr < kernel_start + kernel_length)
	  || (addr_end > kernel_start && addr_end <= kernel_start + kernel_length)) {
	kprintf("%#lx-%#lx overlaps the kernel.\n", addr, addr_end);
	return true;
  }
  else if(addr >= (uint64_t)&ktcb_start && addr < (uint64_t)&ktcb_end) {
	kprintf("%#lx-%#lx overlaps the TCB.\n", addr, addr_end);
	return true;
  }
  else {
	int in_some_region = 0;

	while(tags->type != MULTIBOOT_TAG_TYPE_END) {
	  switch(tags->type) {
		case MULTIBOOT_TAG_TYPE_MMAP: {
		  const struct multiboot_tag_mmap *mmap =
			  (const struct multiboot_tag_mmap*)tags;
		  const struct multiboot_mmap_entry *entry =
			  (const struct multiboot_mmap_entry*)mmap->entries;
		  size_t offset = offsetof(struct multiboot_tag_mmap, entries);

		  while(offset < mmap->size) {
			if((addr >= entry->addr && addr < entry->addr + entry->len)
				|| (addr_end > entry->addr
					&& addr_end < entry->addr + entry->len)) {
			  in_some_region = 1;

			  switch(entry->type) {
				case MULTIBOOT_MEMORY_AVAILABLE:
				  break;
				default:
					kprintf("%#lx-%#lx is in reserved memory.\n", addr, addr_end);
				  return true;
			  }
			}

			offset += mmap->entry_size;
			entry = (const struct multiboot_mmap_entry*)((uintptr_t)entry
				+ mmap->entry_size);
		  }

		  if(!in_some_region) {
			kprintf("%#lx-%#lx lies outside of the memory map.\n", addr, addr_end);
			return true;
		  }

		  break;
		}
		case MULTIBOOT_TAG_TYPE_MODULE: {
		  const struct multiboot_tag_module *module =
			  (const struct multiboot_tag_module*)tags;

		  if((addr >= module->mod_start && addr < module->mod_end)
			  || (addr_end > module->mod_start && addr_end <= module->mod_end))
		  {
			kprintf("%#lx-%#lx overlaps a module.\n", addr, addr_end);

			return true;
		  }

		  break;
		}
		default:
		  break;
		case MULTIBOOT_TAG_TYPE_END:
		  return false;
	  }

	  tags = (const struct multiboot_tag*)((uintptr_t)tags + tags->size + ((tags->size % 8) == 0 ? 0 : 8 - (tags->size % 8)));
	}

	return false;
  }
}

void setup_gdt(void) {
  volatile tss64_entry_t *tss_desc = (volatile tss64_entry_t *)&kernel_gdt[TSS_SEL / sizeof(gdt_entry_t)];
  struct pseudo_descriptor gdt_pointer = {
	.base = (uint64_t)kernel_gdt,
	.limit = 7 * sizeof(gdt_entry_t)
  };

  size_t tss_limit = sizeof tss;
  tss_desc->base1 = (uint16_t)((uintptr_t)&tss & 0xFFFFul);
  tss_desc->base2 = (uint8_t)(((uintptr_t)&tss >> 16) & 0xFFul);
  tss_desc->base3 = (uint8_t)(((uintptr_t)&tss >> 24) & 0xFFul);
  tss_desc->access_flags = GDT_SYS | GDT_TSS | GDT_DPL0 | GDT_PRESENT;
  tss_desc->flags2 = GDT_BYTE_GRAN;
  tss_desc->limit1 = (uint16_t)(tss_limit & 0xFFFFu); // Size of TSS structure and IO Bitmap (in pages)
  tss_desc->limit2 = (uint8_t)(tss_limit >> 16);
  tss_desc->base4 = (uint32_t)(((uintptr_t)&tss >> 32) & 0xFFFFFFFFul);
  tss_desc->_resd = 0;

  __asm__ __volatile__("lgdt %0" :: "m"(gdt_pointer) : "memory");
  __asm__ __volatile__("ltr %%ax" :: "a"(TSS_SEL));
}

NON_NULL_PARAMS int init_memory(const struct multiboot_info_header *header) {
  first_free_page = (addr_t)&ktcb_end;

  setup_gdt();

  if(num_paging_levels <= 2)
	RET_MSG(E_FAIL, "num_paging_levels must be greater than 2");

  const struct multiboot_tag *tags = header->tags;

  mb_info_header = header;

  while(tags->type != MULTIBOOT_TAG_TYPE_END) {
	switch(tags->type) {
	  case MULTIBOOT_TAG_TYPE_BASIC_MEMINFO: {
		const struct multiboot_tag_basic_meminfo *meminfo =
			(const struct multiboot_tag_basic_meminfo*)tags;

		unsigned long int total_phys_mem = (meminfo->mem_upper + 1024) * 1024;

		if(total_phys_mem < (MIN_MEM_MB_REQ * 1048576)) {
		  kprintf(
			  "Not enough memory. System must have at least %u MiB of memory, but only %lu bytes are available.\n",
			  MIN_MEM_MB_REQ,
			  total_phys_mem);
		  return E_FAIL;
		}

		return E_OK;
	  }
	  case MULTIBOOT_TAG_TYPE_END:
		break;
	  default:
		tags = (const struct multiboot_tag*)((uintptr_t)tags + tags->size + ((tags->size % 8) == 0 ? 0 : 8 - (tags->size % 8)));
		break;
	}
  }

  RET_MSG(E_FAIL, "Unable to determine total system memory.");
}
