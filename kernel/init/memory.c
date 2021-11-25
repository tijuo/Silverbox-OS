#include "init.h"
#include "memory.h"
#include <kernel/multiboot.h>

DISC_DATA addr_t firstFreePage;

DISC_CODE addr_t allocPageFrame(void);

DISC_CODE bool isReservedPage(uint64_t addr, multiboot_info_t *info,
                              int isLargePage);

DISC_CODE int clearPhysPage(uint64_t phys);
DISC_CODE int initMemory(multiboot_info_t *info);
DISC_CODE static void setupGDT(void);
DISC_CODE void initPaging(void);

DISC_DATA ALIGNED(PAGE_SIZE) pmap_entry_t kPageDir[PMAP_ENTRY_COUNT]; // The initial page directory used by the kernel on bootstrap

extern pte_t kMapAreaPTab[PTE_ENTRY_COUNT];
extern gdt_entry_t kernelGDT[8];
extern multiboot_info_t *multibootInfo;

/**
 Zero out a page frame.

 @param phys Physical address frame to clear.
 @return E_OK on success. E_FAIL on failure.
 */

int clearPhysPage(uint64_t phys) {
  pmap_entry_t oldPmapEntry;
  size_t offset = LARGE_PAGE_OFFSET(ALIGN_DOWN(phys, (uint64_t)PAGE_SIZE));

  if(IS_ERROR(mapLargeFrame(phys, &oldPmapEntry)))
    RET_MSG(E_FAIL, "Unable to map physical frame.");

  memset((void*)(KMAP_AREA + offset), 0, PAGE_SIZE);

  if(IS_ERROR(unmapLargeFrame(oldPmapEntry)))
    RET_MSG(E_FAIL, "Unable to unmap physical frame.");

  return E_OK;
}

addr_t allocPageFrame(void) {
  addr_t frame = firstFreePage;

  while(frame >= EXTENDED_MEMORY && isReservedPage(frame, multibootInfo, 0)) {
    frame += PAGE_SIZE;
  }

  if(frame < EXTENDED_MEMORY)
    return INVALID_PFRAME;

  firstFreePage = frame + PAGE_SIZE;

  return frame;
}

bool isReservedPage(uint64_t addr, multiboot_info_t *info, int isLargePage) {
  unsigned int kernelStart = (unsigned int)&kPhysStart;
  unsigned int kernelLength = (unsigned int)kSize;
  uint64_t addrEnd;

  if(isLargePage) {
    addr = addr & ~(PAGE_TABLE_SIZE - 1);
    addrEnd = addr + PAGE_TABLE_SIZE;
  }
  else {
    addr = addr & ~(PAGE_SIZE - 1);
    addrEnd = addr + PAGE_SIZE;
  }

  if(addr < (uint64_t)EXTENDED_MEMORY)
    return true;
  else if((addr >= kernelStart && addr < kernelStart + kernelLength)
      || (addrEnd >= kernelStart && addrEnd < kernelStart + kernelLength))
    return true;
  else if(addr >= (addr_t)EXT_PTR(kTcbStart) && addr < (addr_t)EXT_PTR(kTcbEnd))
    return true;
  else {
    int inSomeRegion = 0;
    const memory_map_t *mmap;

    for(unsigned int offset = 0; offset < info->mmap_length;
        offset += mmap->size + sizeof(mmap->size))
    {
      mmap = (const memory_map_t*)KPHYS_TO_VIRT(info->mmap_addr + offset);

      uint64_t mmapLen = mmap->length_high;
      mmapLen = mmapLen << 32;
      mmapLen |= mmap->length_low;

      uint64_t mmapBase = mmap->base_addr_high;
      mmapBase = mmapBase << 32;
      mmapBase |= mmap->base_addr_low;

      if(((uint64_t)addr >= mmapBase && (uint64_t)addr <= mmapBase + mmapLen) || ((uint64_t)addrEnd
          >= mmapBase
                                                                                  && (uint64_t)addrEnd <= mmapBase
                                                                                      + mmapLen))
      {
        inSomeRegion = 1;

        if(mmap->type != MBI_TYPE_AVAIL)
          return true;
        else
          break;
      }
    }

    if(!inSomeRegion)
      return true;

    module_t *module = (module_t*)KPHYS_TO_VIRT(info->mods_addr);

    for(unsigned int i = 0; i < info->mods_count; i++, module++) {
      if(addr > module->mod_start && addr >= module->mod_end
         && addrEnd > module->mod_start && addrEnd > module->mod_end)
        continue;
      else if(addr < module->mod_start && addr < module->mod_end
              && addrEnd <= module->mod_start && addrEnd < module->mod_end)
        continue;
      else
        return true;
    }

    return false;
  }
}

void setupGDT(void) {
  gdt_entry_t *tssDescriptor = &kernelGDT[TSS_SEL / sizeof(gdt_entry_t)];
  struct GdtPointer gdtPointer = {
    .base = (uint32_t)kernelGDT,
    .limit = 6 * sizeof(gdt_entry_t)
  };

  size_t tssLimit = sizeof tss;
  tssDescriptor->base1 = (uint32_t)&tss & 0xFFFFu;
  tssDescriptor->base2 = (uint8_t)(((uint32_t)&tss >> 16) & 0xFFu);
  tssDescriptor->base3 = (uint8_t)(((uint32_t)&tss >> 24) & 0xFFu);
  tssDescriptor->limit1 = (uint16_t)(tssLimit & 0xFFFF); // Size of TSS structure and IO Bitmap (in pages)
  tssDescriptor->limit2 = (uint8_t)(tssLimit >> 16);

  // Set the single kernel stack that will be shared between threads

  tss.ss0 = KDATA_SEL;
  tss.esp0 = (uint32_t)kernelStackTop;

  __asm__ __volatile__("lgdt %0" :: "m"(gdtPointer) : "memory");
  __asm__ __volatile__("ltr %%ax" :: "a"(TSS_SEL));
}

int initMemory(multiboot_info_t *info) {
  firstFreePage = KVIRT_TO_PHYS((addr_t )&kTcbEnd);

  unsigned int totalPhysMem = (info->mem_upper + 1024) * 1024;

  if(IS_FLAG_SET(info->flags, MBI_FLAGS_MEM))
    kprintf("Lower Memory: %d bytes Upper Memory: %d bytes\n",
            info->mem_lower << 10, info->mem_upper << 10);
  kprintf("Total Memory: %#x. %d pages.\n", totalPhysMem,
          totalPhysMem / PAGE_SIZE);

  if(totalPhysMem < (64 << 20)) {
    kprintf("Not enough memory. System must have at least 64 MiB of memory.\n");
    return E_FAIL;
  }

  kprintf("Kernel AddrSpace: %#p\n", kPageDir);

#ifdef DEBUG

  kprintf("TCB Table size: %d bytes\n", kTcbTableSize);
#endif /* DEBUG */

  return E_OK;
}

// Only code/data in the .boot segment can be accesesed without faulting until paging is
// enabled.

void initPaging(void) {
  uint32_t oldCR0;
  pmap_entry_t *mapAreaPTab = (pmap_entry_t *)KVIRT_TO_PHYS(kMapAreaPTab);

  // These are supposed to be cleared, but just in case...

  for(size_t i=0; i < 1024; i++)
    kPageDir[i].value = 0;

  for(size_t i=0; i < 1024; i++)
    mapAreaPTab[i].value = 0;

  large_pde_t *pde = &kPageDir[0].largePde;

  pde->baseLower = 0;
  pde->baseUpper = 0;
  pde->isLargePage = 1;
  pde->isPresent = 1;
  pde->isReadWrite = 1;

  /* Map lower 256 MiB of physical memory to kernel space.
   * The kernel has to be careful not to write to read-only or non-existent areas.
   */

  for(addr_t addr = ALIGN_DOWN(KERNEL_VSTART, LARGE_PAGE_SIZE);
      addr < KERNEL_VEND && addr >= ALIGN_DOWN(KERNEL_VSTART, LARGE_PAGE_SIZE);
      addr +=
      LARGE_PAGE_SIZE)
  {
    size_t pdeIndex = PDE_INDEX(addr);
    large_pde_t *largePde = &kPageDir[pdeIndex].largePde;
    pframe_t pframe = ADDR_TO_PFRAME(KVIRT_TO_PHYS(addr));

    largePde->global = 1;
    largePde->baseLower = (uint32_t)((pframe >> 10) & 0x3FFu);
    largePde->baseUpper = (uint32_t)((pframe >> 20) & 0xFFu);

    largePde->isLargePage = 1;
    largePde->isReadWrite = 1;
    largePde->isPresent = 1;
  }

  pde_t *kMapPde = &kPageDir[PDE_INDEX(KMAP_AREA2)].pde;

  kMapPde->base = ADDR_TO_PFRAME(KVIRT_TO_PHYS(kMapAreaPTab));

  kMapPde->isReadWrite = 1;
  kMapPde->isPresent = 1;

  // Set the page directory

  __asm__("mov %0, %%cr3" :: "r"((uint32_t)kPageDir));

  // Enable debugging extensions, large pages, global pages,
  // FXSAVE/FXRSTOR, and SIMD exceptions

  __asm__("mov %0, %%cr4" :: "r"(CR4_DE | CR4_PSE | CR4_PGE
          | CR4_OSFXSR | CR4_OSXMMEXCPT));

  // Enable paging

  __asm__("mov %%cr0, %0" : "=r"(oldCR0));
  __asm__("mov %0, %%cr0" :: "r"(oldCR0 | CR0_PG | CR0_MP) : "memory");


  setupGDT();

  // Reload segment registers

  __asm__("mov %0, %%ds\n"
      "mov %0, %%es\n"
      "mov %0, %%ss\n"
      "mov %1, %%fs\n"
      "mov %1, %%gs\n" :: "r"(KDATA_SEL), "r"(0) : "memory");

  __asm__("push %%eax\n"
          "pushl %0\n"
          "mov $1f, %%eax\n"
          "push %%eax\n"
          "retf\n"
          "1:\n"
          "pop %%eax\n" :: "i"(KCODE_SEL) : "memory", "cc");
}
