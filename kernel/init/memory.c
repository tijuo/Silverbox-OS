#include "init.h"
#include "memory.h"
#include <kernel/multiboot2.h>
#include <kernel/pae.h>
#include <kernel/memory.h>
#include <util.h>
#include <kernel/algo.h>
#include <kernel/lowlevel.h>
#include <kernel/mm.h>

_Static_assert(sizeof(size_t) == 8, "size_t is not 8 bytes.");
_Static_assert(sizeof(long int) == 8, "long int is not 8 bytes.");
_Static_assert(sizeof(unsigned long int) == 8,
               "unsigned long int is not 8 bytes.");

size_t total_phys_memory;
extern gdt_entry_t kernel_gdt[];

enum memory_map_type {
  AVAILABLE,
  ACPI,
  ACPI_NVS,
  RESERVED,
  KERNEL,
  MODULE,
  MMIO,
  UNUSABLE,
  UNKNOWN,
  HOLE
};

struct memory_map {
  paddr_t start;
  size_t length;
  enum memory_map_type type;
};

struct resd_block {
  paddr_t start;
  size_t length;
  uint64_t *bitmap;
};

struct module {
  paddr_t start;
  size_t length;
  uint8_t command[64];
};

#define MEM_MAP_MAX_COUNT   128
#define MAX_MODULE_COUNT    64
#define MAX_RESD_REGIONS    128

#define MIN_MEM_MB_REQ      64

extern int kfree_buf_node_constructor(void *buf, size_t size);
extern int kmem_cache_constructor(void *buf, size_t size);
extern int kmem_cache_destructor(void *buf, size_t size);

DISC_CODE addr_t alloc_page_frame(void);
DISC_CODE static void setup_gdt(void);
DISC_CODE static int memory_map_compare(const void *x1, const void *x2);

int memory_map_compare(const void *x1, const void *x2) {
  const struct memory_map *m1 = (const struct memory_map *)x1;
  const struct memory_map *m2 = (const struct memory_map *)x2;

  if(m1->start < m2->start)
    return -1;
  else if(m1->start > m2->start)
    return 1;
  else
    return 0;
}

void setup_gdt(void)
{
  volatile tss64_entry_t *tss_desc = (volatile tss64_entry_t *)
                                     &kernel_gdt[TSS_SEL / sizeof(gdt_entry_t)];
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
  tss_desc->limit1 = (uint16_t)(tss_limit &
                                0xFFFFu); // Size of TSS structure and IO Bitmap (in pages)
  tss_desc->limit2 = (uint8_t)(tss_limit >> 16);
  tss_desc->base4 = (uint32_t)(((uintptr_t)&tss >> 32) & 0xFFFFFFFFul);
  tss_desc->_resd = 0;

  __asm__ __volatile__("lgdt %0" :: "m"(gdt_pointer) : "memory");
  __asm__ __volatile__("ltr %%ax" :: "a"(TSS_SEL));
}

int init_memory(const struct multiboot_info_header *header)
{
  struct memory_map mem_map[MEM_MAP_MAX_COUNT];
  struct module boot_modules[MAX_MODULE_COUNT];
  struct memory_block resd_blocks[MAX_RESD_REGIONS];

  size_t map_count = 0;
  size_t module_count = 0;
  size_t resd_count = 0;
  paddr_t addr = 0;

  //first_free_page = (addr_t)&ktcb_end;

  setup_gdt();

  if(num_paging_levels <= 2)
    RET_MSG(E_FAIL, "num_paging_levels must be greater than 2");

  const struct multiboot_tag *tags = header->tags;

  //mb_info_header = header;

  while(tags->type != MULTIBOOT_TAG_TYPE_END) {
    switch(tags->type) {
      case MULTIBOOT_TAG_TYPE_MMAP: {
          const struct multiboot_tag_mmap *mmap =
            (const struct multiboot_tag_mmap *)tags;
          const struct multiboot_mmap_entry *entry =
            (const struct multiboot_mmap_entry *)mmap->entries;
          size_t offset = offsetof(struct multiboot_tag_mmap, entries);

          while(offset < mmap->size) {
            if(map_count < MEM_MAP_MAX_COUNT) {
              if((entry->type == MULTIBOOT_MEMORY_AVAILABLE
                  || entry->type == MULTIBOOT_MEMORY_ACPI_RECLAIMABLE)
                  && entry->addr + entry->len > total_phys_memory)
                total_phys_memory = entry->addr + entry->len;

              mem_map[map_count].start = entry->addr;
              mem_map[map_count].length = entry->len;

              switch(entry->type) {
                case MULTIBOOT_MEMORY_AVAILABLE:
                  mem_map[map_count].type = AVAILABLE;
                  break;
                case MULTIBOOT_MEMORY_ACPI_RECLAIMABLE:
                  mem_map[map_count].type = ACPI;
                  break;
                case MULTIBOOT_MEMORY_BADRAM:
                  mem_map[map_count].type = UNUSABLE;
                  break;
                case MULTIBOOT_MEMORY_NVS:
                  mem_map[map_count].type = ACPI_NVS;
                  break;
                case MULTIBOOT_MEMORY_RESERVED:
                  mem_map[map_count].type = RESERVED;
                  break;
                default:
                  mem_map[map_count].type = UNKNOWN;
                  break;
              }

              map_count++;
            } else
              RET_MSG(E_FAIL, "Too many memory map entries.");

            offset += mmap->entry_size;
            entry = (const struct multiboot_mmap_entry *)((uintptr_t)entry
                    + mmap->entry_size);
          }
          break;
        }
      case MULTIBOOT_TAG_TYPE_MODULE: {
          const struct multiboot_tag_module *module = (const struct multiboot_tag_module *)tags;

          boot_modules[module_count].start = module->mod_start;
          boot_modules[module_count].length = module->mod_end - module->mod_start;

          if(module->cmdline)
            kstrncpy((char *)boot_modules[module_count].command, module->cmdline, sizeof boot_modules[module_count].command);
          module_count++;

          kprintf("Module: load addr: %p size: %u command: \"%s\"\n",
                  (void *)(uintptr_t)module->mod_start, module->mod_end - module->mod_start,
                  module->cmdline);

          break;
        }
      default:
      case MULTIBOOT_TAG_TYPE_END:
        break;
    }

    if(tags->type != MULTIBOOT_TAG_TYPE_END) {
      // Each tag must be aligned to an 8-byte boundary.
      tags = (const struct multiboot_tag *)((uintptr_t)tags + tags->size + ((
                                              tags->size % 8) == 0 ? 0 : 8 - (tags->size % 8)));
    }
  }

  if(map_count == 0)
    RET_MSG(E_FAIL, "Unable to determine total system memory.");
  else {
    size_t j = 0;

    /* Search for the kernel region in the memory map, then split the
       memory mapping into (at most) three: one for the kernel region, and
       the other two is the set difference between the previous memory map region
       and the kernel region. */

    for(size_t i = 0; i < map_count; i++) {
      if((paddr_t)&kcode >= mem_map[i].start && (paddr_t)&kcode < mem_map[i].start + mem_map[i].length
          && (paddr_t)&kend > mem_map[i].start && (paddr_t)&kend <= mem_map[i].start + mem_map[i].length) {
        if((paddr_t)&kcode - mem_map[i].start > 0) {
          if(map_count + j + 1 >= MEM_MAP_MAX_COUNT)
            RET_MSG(E_FAIL, "Too many memory map entries. Unable to add memory region.");

          mem_map[map_count + j].start = mem_map[i].start;
          mem_map[map_count + j].length = (paddr_t)&kcode - (paddr_t)mem_map[i].start;
          mem_map[map_count + j].type = mem_map[i].type;
          j++;
        }

        if(mem_map[i].start + mem_map[i].length - (paddr_t)&kend > 0) {
          if(map_count + j + 1 >= MEM_MAP_MAX_COUNT)
            RET_MSG(E_FAIL, "Too many memory map entries. Unable to add memory region.");

          mem_map[map_count + j].start = (paddr_t)&kend;
          mem_map[map_count + j].length = mem_map[i].start + mem_map[i].length - (paddr_t)&kend;
          mem_map[map_count + j].type = mem_map[i].type;
          j++;
        }

        mem_map[i].start = (paddr_t)&kcode;
        mem_map[i].length = (paddr_t)&kend - (paddr_t)&kcode;
        mem_map[i].type = KERNEL;

        map_count += j;
      }
    }

    kqsort(mem_map, map_count, sizeof(struct memory_map), memory_map_compare);

    // Now do the same thing, but for each module

    for(size_t m = 0; m < module_count; m++) {
      j = 0;
      const struct module *mod = (const struct module *)&boot_modules[m];

      for(size_t i = 0; i < map_count; i++) {
        if(mod->start >= mem_map[i].start && mod->start < mem_map[i].start + mem_map[i].length
            && (mod->start + mod->length) > mem_map[i].start
            && (mod->start + mod->length) <= mem_map[i].start + mem_map[i].length) {
          if(mod->start - mem_map[i].start > 0) {
            if(map_count + j + 1 >= MEM_MAP_MAX_COUNT)
              RET_MSG(E_FAIL, "Too many memory map entries. Unable to add memory region.");

            mem_map[map_count + j].start = mem_map[i].start;
            mem_map[map_count + j].length = (paddr_t)mod->start - (paddr_t)mem_map[i].start;
            mem_map[map_count + j].type = mem_map[i].type;
            j++;
          }

          if(mem_map[i].start + mem_map[i].length - (mod->start + mod->length) > 0) {
            if(map_count + j + 1 >= MEM_MAP_MAX_COUNT)
              RET_MSG(E_FAIL, "Too many memory map entries. Unable to add memory region.");

            mem_map[map_count + j].start = (paddr_t)(mod->start + mod->length);
            mem_map[map_count + j].length = mem_map[i].start + mem_map[i].length - (paddr_t)(mod->start + mod->length);
            mem_map[map_count + j].type = mem_map[i].type;
            j++;
          }

          mem_map[i].start = (paddr_t)mod->start;
          mem_map[i].length = (paddr_t)mod->length;
          mem_map[i].type = MODULE;

          map_count += j;
        }
      }

      kqsort(mem_map, map_count, sizeof(struct memory_map), memory_map_compare);
    }
    // Add hole regions

    j = 0;

    for(size_t i = 0; i < map_count; i++) {
      if(mem_map[i].start != addr) {
        if(map_count + j + 1 >= MEM_MAP_MAX_COUNT)
          RET_MSG(E_FAIL,
                  "Too many memory map entries. Unable to add memory hole entry.");

        mem_map[map_count + j].start = addr;
        mem_map[map_count + j].length = mem_map[i].start - addr;
        mem_map[map_count + j].type = HOLE;
        j++;
      }

      addr = mem_map[i].start + mem_map[i].length;
    }

    if(map_count + j + 1 >= MEM_MAP_MAX_COUNT)
      RET_MSG(E_FAIL,
              "Too many memory map entries. Unable to add memory hole entry.");

    mem_map[map_count + j].start = addr;
    mem_map[map_count + j].length = 1 + (max_phys_addr - addr);
    mem_map[map_count + j].type = HOLE;
    j++;
    addr = max_phys_addr + 1;

    if(addr != 0) {
      if(map_count + j + 1 >= MEM_MAP_MAX_COUNT)
        RET_MSG(E_FAIL,
                "Too many memory map entries. Unable to add memory hole entry.");

      mem_map[map_count + j].start = addr;
      mem_map[map_count + j].length = -addr;
      mem_map[map_count + j].type = UNUSABLE;
      j++;
    }

    map_count += j;
    kqsort(mem_map, map_count, sizeof(struct memory_map), memory_map_compare);
  }

  kprintf("Memory map: \n\n");

  const char *type_name[] = { "Available", "ACPI", "ACPI NVS", "Reserved",
                              "Kernel", "Boot Module", "MMIO", "Unusable", "Unknown",
                              "Hole"
                            };

  for(size_t i = 0; i < map_count; i++) {
    kprintf("%3lu: %p:%p %s\n", i, (void *)mem_map[i].start,
            (void *)(mem_map[i].start + mem_map[i].length - 1), type_name[mem_map[i].type]);
  }

  kprintf("\nTotal physical memory: %lu bytes\n", total_phys_memory);

  /*
  if(IS_ERROR(buddy_init(&phys_allocator, total_phys_memory, PAE_LARGE_PAGE_SIZE, free_phys_blocks)))
    RET_MSG(E_FAIL, "Unable to initialize physical memory allocator.");

  addr = 0;
*/

  list_init(&free_frame_list);
  // list_init(&dma_free_frame_list);

  for(size_t i = 0; i < map_count; i++) {
    if(mem_map[i].type == AVAILABLE) {
      size_t padding = ALIGN_UP(mem_map[i].start, PAGE_SIZE) - mem_map[i].start;

      if(mem_map[i].length < padding + PAGE_SIZE)
        continue;
/*
      if(is_list_empty(&dma_free_frame_list)) {
        if(mem_map[i].length >= MIN_DMA_MEM)
      }
*/
      list_node_t *node = (list_node_t *)(mem_map[i].start + padding);

      node->item.item_void_ptr = (void *)(mem_map[i].start + mem_map[i].length);

      list_insert_node_at_end(&free_frame_list, true, node);
    }
/*
    paddr_t region_end = mem_map[i].start + mem_map[i].length;

    if(mem_map[i].type != AVAILABLE) {
      if(!IS_ERROR(buddy_reserve_block(&phys_allocator, (void *)mem_map[i].start,
                                       mem_map[i].length,
                                       &mem_block))) {
        if(resd_count >= MAX_RESD_REGIONS)
          RET_MSG(E_FAIL, "Unable to add reserved region.");
        resd_blocks[resd_count++] = mem_block;
      }
    }

    addr = region_end;
    */
  }

  kmem_cache_init(&kbuf_node_mem_cache, "buf_node", sizeof(struct kfree_buf_node), 0, kfree_buf_node_constructor, NULL);
  kmem_cache_init(&kmem_cache_mem_cache, "mem_cache", sizeof(struct kmem_cache), 0, kmem_cache_constructor, kmem_cache_destructor);

  return E_OK;
}
