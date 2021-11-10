#include <elf.h>

#include <kernel/message.h>
#include <kernel/interrupt.h>
#include <kernel/thread.h>
#include <kernel/debug.h>
#include <kernel/mm.h>
#include <kernel/schedule.h>
#include <kernel/paging.h>
#include <kernel/memory.h>
#include <kernel/mm.h>
#include <oslib.h>
#include <kernel/error.h>
#include <os/msg/init.h>
#include <os/syscalls.h>
#include <kernel/bits.h>
#include <kernel/syscall.h>
#include <kernel/io.h>
#include <kernel/lowlevel.h>
#include <cpuid.h>
#include <x86gprintrin.h>

#define KERNEL_IDT_LEN	(64 * sizeof(struct IdtEntry))

#define RSDP_SIGNATURE  "RSD PTR "
#define PARAGRAPH_LEN   16

struct RSDPointer {
  char signature[8]; // should be "RSD PTR "
  uint8_t checksum;
  char oemId[6];
  uint8_t revision;
  uint32_t rsdtAddress; // physical address
  uint32_t length;
  uint64_t xsdtAddress; // physical address
  uint8_t extChecksum;
  char _resd[3];
} PACKED;

struct ACPI_DT_Header {
  char signature[4];
  uint32_t length;
  uint8_t revision;
  uint8_t checksum;
  char oemId[6];
  char oemTableId[8];
  uint32_t oemRevision;
  uint32_t creatorId;
  uint32_t creatorRevision;
};

#define MP_SIGNATURE    "_MP_"

struct MP_FloatingHeader {
  char signature[4]; // should be "_MP_"
  uint32_t tableAddress; // physical address
  uint8_t length; // 16 bytes
  uint8_t specRev;
  uint8_t checksum;
  uint8_t mpConfigType;
  uint8_t _resd :7;
  uint8_t icmrPresent :1;
  uint8_t _resd2[3];
};

#define MP_CT_SIGNATURE "PCMP"

struct MP_CT_Header {
  char signature[4]; // should be PCMP
  uint16_t length;
  uint8_t revision;
  uint8_t checksum;
  char oemId[8];
  char productId[12];
  uint32_t oemTableAddress; // physical address
  uint16_t oemTableSize;
  uint16_t numEntries;
  uint32_t lapicAddress; // physical address
  uint16_t extTableLength;
  uint8_t extChecksum;
  uint8_t _resd;
};

#define MP_ENTRY_PROCESSOR  0
#define MP_ENTRY_BUS        1
#define MP_ENTRY_IO_APIC    2
#define MP_ENTRY_IO_INT     3
#define MP_ENTRY_LOCAL_INT  4

struct MP_Processor {
  uint8_t entryType; // 0
  uint8_t lapicId;
  uint8_t lapicVersion;
  uint8_t isEnabled :1;
  uint8_t isBootstrap :1;
  uint8_t _resd :6;
  uint32_t cpuSignature;
  uint32_t featureFlags;
  uint32_t _resd2;
  uint32_t _resd3;
} PACKED;

#define EBDA                0x80000u
#define VGA_RAM             0xA0000u
#define VGA_COLOR_TEXT      0xB8000u
#define ISA_EXT_ROM         0xC0000u
#define BIOS_EXT_ROM        0xE0000u
#define BIOS_ROM            0xF0000u
#define EXTENDED_MEMORY     0x100000u

#define DISC_CODE(X) X SECTION(".dtext")

#define  DISC_DATA(X)  X SECTION(".ddata")

#define INIT_SERVER_FLAG	"initsrv="

extern pte_t kMapAreaPTab[PTE_ENTRY_COUNT];

DISC_DATA(uint8_t kBootStack[4 * PAGE_SIZE]) ALIGNED(PAGE_SIZE);
DISC_DATA(void *kBootStackTop) = kBootStack + sizeof kBootStack;

static inline void enterContext(paddr_t addrSpace, ExecutionState state) {
  ExecutionState *s = &state;

  __asm__ __volatile__(
      "mov %0, %%esp\n"
      "mov %1, %%cr3\n"
      "pop %%edi\n"
      "pop %%esi\n"
      "pop %%ebp\n"
      "pop %%ebx\n"
      "pop %%edx\n"
      "pop %%ecx\n"
      "pop %%eax\n"
      "iret\n" :: "r"(s), "r"(addrSpace) : "memory");
}

DISC_DATA(pmap_entry_t kPageDir[PMAP_ENTRY_COUNT]) ALIGNED(PAGE_SIZE); // The initial page directory used by the kernel on bootstrap
DISC_DATA(pte_t kPageTab[PTE_ENTRY_COUNT]) ALIGNED(PAGE_SIZE);

extern gdt_entry_t kernelGDT[8];
extern idt_entry_t kernelIDT[NUM_EXCEPTIONS + NUM_IRQS];

extern tcb_t *initServerThread;
extern addr_t *freePageStack;
extern addr_t *freePageStackTop;
extern uint8_t *kernelStackTop;

//static bool DISC_CODE(isValidAcpiHeader(paddr_t physAddress));
DISC_CODE(void initPaging(void));
static tcb_t* DISC_CODE(loadElfExe( addr_t, uint32_t, void * ));
static bool DISC_CODE(isValidElfExe( elf_header_t *image ));
static void DISC_CODE(initInterrupts( void ));
//static void DISC_CODE(initTimer( void ));
static int DISC_CODE(initMemory( multiboot_info_t *info ));
static void DISC_CODE(setupGDT(void));
static void DISC_CODE(stopInit(const char *));
static void DISC_CODE(bootstrapInitServer(multiboot_info_t *info));
void DISC_CODE(init(multiboot_info_t *));
//static void DISC_CODE(initPIC( void ));
//static int DISC_CODE(memcmp(const char *s1, const char *s2, register size_t n));
static int DISC_CODE(strncmp(const char *, const char *, size_t num));
//static size_t DISC_CODE(strlen(const char *s));
static char* DISC_CODE(strstr(char *, const char *));
static char* DISC_CODE(strchr(char *, int));
void DISC_CODE(addIDTEntry(void (*f)(void), unsigned int entryNum, unsigned int dpl));
void DISC_CODE(loadIDT(void));

static void DISC_CODE(initStructures(multiboot_info_t *));
/*
 static unsigned DISC_CODE(bcd2bin(unsigned num));
 static unsigned long long DISC_CODE(mktime(unsigned int year, unsigned int month, unsigned int day, unsigned int hour,
 unsigned int minute, unsigned int second));
 */
static addr_t DISC_DATA(initServerImg);
static bool DISC_CODE(isReservedPage(paddr_t addr, multiboot_info_t * info,
        int isLargePage));

//paddr_t DISC_CODE(findRSDP(struct RSDPointer *header));
//paddr_t DISC_CODE(findMP_Header(struct MP_FloatingHeader *header));
//static void DISC_CODE( readPhysMem(addr_t address, addr_t buffer, size_t len) );
static void DISC_CODE(initPageAllocator(multiboot_info_t * info));
DISC_DATA(static addr_t lastKernelFreePage);

DISC_DATA(static void (*cpuExHandlers[32])(void)) = {
  cpuEx0Handler, cpuEx1Handler, cpuEx2Handler, cpuEx3Handler, cpuEx4Handler, cpuEx5Handler,
  cpuEx6Handler, cpuEx7Handler, cpuEx8Handler, cpuEx9Handler, cpuEx10Handler, cpuEx11Handler,
  cpuEx12Handler, cpuEx13Handler, cpuEx14Handler, cpuEx15Handler, cpuEx16Handler, cpuEx17Handler,
  cpuEx18Handler, cpuEx19Handler, cpuEx20Handler, cpuEx21Handler, cpuEx22Handler, cpuEx23Handler,
  cpuEx24Handler, cpuEx25Handler, cpuEx26Handler, cpuEx27Handler, cpuEx28Handler, cpuEx29Handler,
  cpuEx30Handler, cpuEx31Handler};

DISC_DATA(static void (*irqIntHandlers[32])(void)) = {
  irq0Handler, irq1Handler, irq2Handler, irq3Handler, irq4Handler, irq5Handler,
  irq6Handler, irq7Handler, irq8Handler, irq9Handler, irq10Handler, irq11Handler,
  irq12Handler, irq13Handler, irq14Handler, irq15Handler, irq16Handler, irq17Handler,
  irq18Handler, irq19Handler, irq20Handler, irq21Handler, irq22Handler, irq23Handler,
  irq24Handler, irq25Handler, irq26Handler, irq27Handler, irq28Handler, irq29Handler,
  irq30Handler, irq31Handler};

/*
 void readPhysMem(addr_t address, addr_t buffer, size_t len)
 {
 unsigned offset, bytes, i=0;

 while( len )
 {
 offset = (unsigned)address & (PAGE_SIZE - 1);
 bytes = (len > PAGE_SIZE - offset) ? PAGE_SIZE - offset : len;

 mapTemp( address & ~0xFFFu );

 memcpy( (void *)(buffer + i), (void *)(TEMP_PAGEADDR + offset), bytes);

 unmapTemp();

 address += bytes;
 i += bytes;
 len -= bytes;
 }
 }
 */

// Assumes 0xE0000 - 0x100000 has been 1:1 mapped
/*
 paddr_t findRSDP(struct RSDPointer *rsdPointer)
 {
 paddr_t retPtr = (paddr_t)NULL;

 for(addr_t addr=BIOS_EXT_ROM; addr < EXTENDED_MEMORY; addr += PAGE_SIZE)
 kMapPage(addr, (paddr_t)addr, PAGING_RO);

 for(uint8_t *ptr=(uint8_t *)BIOS_EXT_ROM; ptr < (uint8_t *)EXTENDED_MEMORY; ptr += PARAGRAPH_LEN)
 {
 if(memcmp((void *)ptr, RSDP_SIGNATURE, sizeof rsdPointer->signature) == 0)
 {
 uint8_t checksum = 0;

 for(size_t i=0; i < offsetof(struct RSDPointer, length); i++)
 checksum += ptr[i];

 if(checksum == 0)
 {
 memcpy(rsdPointer, ptr, sizeof *rsdPointer);
 retPtr = (paddr_t)(uintptr_t)ptr;
 break;
 }
 }
 }

 for(addr_t addr=BIOS_EXT_ROM; addr < EXTENDED_MEMORY; addr += PAGE_SIZE)
 kUnmapPage(addr, NULL);

 return retPtr;
 }

 paddr_t findMP_Header(struct MP_FloatingHeader *mpHeader)
 {
 addr_t memoryRegions[4][2] = { {BIOS_ROM, EXTENDED_MEMORY}, {EBDA, EBDA+1024},
 {VGA_RAM-1024, VGA_RAM}, {0x7fc00, 0x80000} };

 paddr_t retPtr = (paddr_t)NULL;

 for(size_t region=0; region < 4; region++)
 {
 for(addr_t addr=memoryRegions[region][0]; addr < memoryRegions[region][1]; addr += PAGE_SIZE)
 {
 if(addr >= VGA_RAM)
 kMapPage(addr, (paddr_t)addr, PAGING_RO);
 }

 for(uint8_t *ptr=(uint8_t *)memoryRegions[region][0]; ptr < (uint8_t *)memoryRegions[region][1]; ptr += PARAGRAPH_LEN)
 {
 if(memcmp((char *)ptr, MP_SIGNATURE, 4) == 0)
 {
 uint8_t checksum = 0;

 for(size_t i=0; i < ((struct MP_FloatingHeader *)ptr)->length; i++)
 checksum += ptr[i];

 if(checksum == 0)
 {
 memcpy(mpHeader, ptr, sizeof *mpHeader);
 retPtr = (paddr_t)(uintptr_t)ptr;
 break;
 }
 }
 }

 for(addr_t addr=memoryRegions[region][0]; addr < memoryRegions[region][1]; addr += PAGE_SIZE)
 {
 if(addr >= VGA_RAM)
 kUnmapPage(addr, NULL);
 }

 if(retPtr)
 break;
 }

 return retPtr;
 }

 bool isValidAcpiHeader(paddr_t physAddress)
 {
 struct ACPI_DT_Header header;
 uint8_t checksum = 0;
 uint8_t buffer[256];
 size_t bytesRead=0;

 if(peek(physAddress, &header, sizeof header) != E_OK)
 return false;

 while(bytesRead < header.length)
 {
 size_t bytesToRead = MIN(header.length - bytesRead, 256u);

 if(peek(physAddress + bytesRead, buffer, bytesToRead) != E_OK)
 return false;

 for(size_t i=0; i < bytesRead; i++)
 checksum += buffer[i];

 bytesRead += bytesToRead;
 }

 return checksum == 0 ? true : false;
 }
 */

bool isReservedPage(paddr_t addr, multiboot_info_t *info, int isLargePage) {
  unsigned int kernelStart = (unsigned int)&kPhysStart;
  unsigned int kernelLength = (unsigned int)&kSize;
  paddr_t addrEnd;

  if(isLargePage) {
    addr = addr & ~(PAGE_TABLE_SIZE - 1);
    addrEnd = addr + PAGE_TABLE_SIZE;
  }
  else {
    addr = addr & ~(PAGE_SIZE - 1);
    addrEnd = addr + PAGE_SIZE;
  }

  if(addr < (paddr_t)EXTENDED_MEMORY)
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

void initPageAllocator(multiboot_info_t *info) {
  const memory_map_t *mmap;

  /* 1. Look for a region of page-aligned contiguous memory
   in the size of a large page (4 MiB) that will
   hold the page stack entries.

   If no such region exists then use small sized pages.
   An extra page will be needed as the page table.

   2. For each 4 MiB region of memory, insert the available
   page frame numbers into the stack. Page frames corresponding
   to reserved memory, kernel memory (incl. page stack), MMIO,
   and boot modules are not added.

   3. Repeat until all available page frames are added to the page
   frame stack.
   */

#ifdef DEBUG
  kprintf("Multiboot memory map:\n");

  for(unsigned int offset = 0; offset < info->mmap_length;
      offset += mmap->size + sizeof(mmap->size))
  {
    mmap = (const memory_map_t*)KPHYS_TO_VIRT(info->mmap_addr + offset);

    uint64_t mmapLen = ((uint64_t)mmap->length_high << 32)
        | (uint64_t)mmap->length_low;
    uint64_t mmapBase = ((uint64_t)mmap->base_addr_high << 32)
        | (uint64_t)mmap->base_addr_low;

    char *mmapType;

    switch(mmap->type) {
      case MBI_TYPE_AVAIL:
        mmapType = "Available";
        break;
      case MBI_TYPE_BAD:
        mmapType = "Defective";
        break;
      case MBI_TYPE_RESD:
        mmapType = "Preserve";
        break;
      case MBI_TYPE_ACPI:
        mmapType = "ACPI";
        break;
      default:
        mmapType = "Reserved";
        break;
    }

    kprintf("%s - Addr: %#llx Length: %#llx\n", mmapType, mmapBase, mmapLen);
  }
#endif

  size_t pageStackSize = sizeof(addr_t) * (info->mem_upper) / (16 * 4);
  size_t stackSizeLeft;

  if(pageStackSize % PAGE_SIZE)
    pageStackSize += PAGE_SIZE - (pageStackSize & (PAGE_SIZE - 1));

  stackSizeLeft = pageStackSize;

  /*
   if(tcbSizeLeft % LARGE_PAGE_SIZE)
   tcbSizeLeft += LARGE_PAGE_SIZE - (tcbSizeLeft & (LARGE_PAGE_SIZE-1));
   */

  kprintf("Page Stack size: %d bytes\n", pageStackSize);

  if(IS_FLAG_SET(info->flags, MBI_FLAGS_MMAP)) {
    addr_t largePages[32];
    addr_t pageTables[32];

    size_t pageTableCount = 0; // Number of addresses corresponding to page tables,
    size_t largePageCount = 0; // large pages,

    unsigned int isPageStackMapped = 0;
    unsigned int smallPageMapCount = 0;
    addr_t mapPtr = (addr_t)freePageStack;

    for(int pageSearchPhase = 0; pageSearchPhase <= 2; pageSearchPhase++) {
      if(isPageStackMapped)
        break;

      for(unsigned int offset = 0; offset < info->mmap_length;
          offset += mmap->size + sizeof(mmap->size))
      {
        if(isPageStackMapped)
          break;

        mmap = (const memory_map_t*)KPHYS_TO_VIRT(info->mmap_addr + offset);

        uint64_t mmapLen = mmap->length_high;
        mmapLen = mmapLen << 32;
        mmapLen |= mmap->length_low;

        if(mmap->type == MBI_TYPE_AVAIL /*&& mmapLen >= pageStackSize*/) {
          uint64_t baseAddr = mmap->base_addr_high;
          unsigned int extraSpace;
          baseAddr = baseAddr << 32;
          baseAddr |= mmap->base_addr_low;

          if(baseAddr >= MAX_PHYS_MEMORY || baseAddr < EXTENDED_MEMORY) // Ignore physical addresses greater than 2 GiB or less than 1 MiB
            continue;

          if(pageSearchPhase == 0) {
            unsigned int largePagesSearched = 0; // Number of 4 MiB pages found in this memory region
            extraSpace = mmap->base_addr_low & (LARGE_PAGE_SIZE - 1);

            if(extraSpace != 0)
              baseAddr += LARGE_PAGE_SIZE - extraSpace; // Make sure the address we'll use is aligned to a 4 MiB boundary.

            // Find and map all large-sized pages in the memory region that we'll need

            while(stackSizeLeft >= LARGE_PAGE_SIZE
                && mmapLen >= extraSpace
                    + (largePagesSearched + 1) * LARGE_PAGE_SIZE)
            {
              if(!isReservedPage(
                  baseAddr + LARGE_PAGE_SIZE * largePagesSearched, info, 1)) {
                largePages[largePageCount] = baseAddr
                    + LARGE_PAGE_SIZE * largePagesSearched;
                stackSizeLeft -= LARGE_PAGE_SIZE;

                //kprintf("Mapping 4 MB page phys 0x%llx to virt %#x\n", largePages[largePageCount], mapPtr);

                kMapPage(mapPtr, largePages[largePageCount],
                PAGING_RW | PAGING_SUPERVISOR | PAGING_4MB_PAGE);
                largePageCount++;
                mapPtr += LARGE_PAGE_SIZE;
              }

              largePagesSearched++;
            }
          }
          else if(pageSearchPhase == 1) // Looking for small page-sized memory
          {
            unsigned int smallPagesSearched = 0; // Number of 4 KiB pages found in this memory region
            unsigned int pageTablesNeeded = 0;
            extraSpace = mmap->base_addr_low & (PAGE_SIZE - 1);

            if(baseAddr % PAGE_SIZE)
              baseAddr += PAGE_SIZE - (baseAddr & (PAGE_SIZE - 1));

            // Map the remaining regions with 4 KiB pages

            while(stackSizeLeft > 0
                && mmapLen >= extraSpace
                    + (pageTablesNeeded + smallPagesSearched + 1) * PAGE_SIZE)
            {
              if(isReservedPage(
                  baseAddr + PAGE_SIZE
                      * (smallPagesSearched + pageTablesNeeded),
                  info, 0)) {
                smallPagesSearched++;
                continue;
              }

              // Locate the memory to be used for the page table

              if(smallPageMapCount % 1024 == 0) {
                if(mmapLen < extraSpace
                    + (pageTablesNeeded + smallPagesSearched + 2) * PAGE_SIZE)
                  break;

                pde_t pde = readPDE(PDE_INDEX(mapPtr), CURRENT_ROOT_PMAP);

                if(!pde.isPresent) {
                  pageTables[pageTableCount] = baseAddr
                      + PAGE_SIZE * (smallPagesSearched + pageTablesNeeded);
                  clearPhysPage(pageTables[pageTableCount]);

                  //kprintf("Mapping page table phys 0x%llx to virt %#x\n", pageTables[pageTableCount], mapPtr);

                  pde.value = pageTables[pageTableCount] | PAGING_RW
                              | PAGING_SUPERVISOR | PAGING_PRES;

                  if(IS_ERROR(
                      writePDE(PDE_INDEX(mapPtr), pde, CURRENT_ROOT_PMAP)))
                    stopInit(
                        "Error while initializing memory: Unable to write PDE.");

                  pageTableCount++;
                  pageTablesNeeded++;
                }
              }

              //kprintf("Mapping page phys 0x%llx to virt %#x\n", baseAddr + PAGE_SIZE * (smallPagesSearched+pageTablesNeeded), mapPtr);

              /*
               * Page was already mapped before init() was called.
               kMapPage(mapPtr, baseAddr + PAGE_SIZE * (smallPagesSearched+pageTablesNeeded),
               PAGING_RW | PAGING_SUPERVISOR);
               */
              stackSizeLeft -= PAGE_SIZE;
              smallPagesSearched++;
              smallPageMapCount++;
              mapPtr += PAGE_SIZE;
            }
          }
        }
      }
    }

    freePageStackTop = freePageStack;

    unsigned int pagesAdded = 0;

    for(size_t offset = 0; offset < info->mmap_length;
        offset += mmap->size + sizeof(mmap->size))
    {
      mmap = (const memory_map_t*)KPHYS_TO_VIRT(info->mmap_addr + offset);

      if(mmap->type == MBI_TYPE_AVAIL) {
        uint64_t mmapLen = mmap->length_high;
        mmapLen = mmapLen << 32;
        mmapLen |= mmap->length_low;

        uint64_t mmapAddr = mmap->base_addr_high;
        mmapAddr = mmapAddr << 32;
        mmapAddr |= mmap->base_addr_low;

        if(mmapAddr & (PAGE_SIZE - 1)) {
          uint64_t extraSpace = PAGE_SIZE - (mmapAddr & (PAGE_SIZE - 1));

          mmapAddr += extraSpace;
          mmapLen -= extraSpace;
        }

        // Check each address in this region to determine if it should be added to the free page list

        for(paddr_t paddr = mmapAddr, end = mmapAddr + mmapLen;
            paddr < MIN(end, 0x100000000u) && pagesAdded
                < pageStackSize / sizeof(addr_t);)
        {
          int found = 0;

          // Is this address within a large page?

          for(size_t i = 0; i < largePageCount; i++) {
            if(paddr >= largePages[i] && paddr < largePages[i] + LARGE_PAGE_SIZE)
            {
              found = 1;
              paddr = largePages[i] + LARGE_PAGE_SIZE;
              break;
            }
          }

          if(found)
            continue;

          // Is this page used as a page table?

          for(size_t i = 0; i < pageTableCount; i++) {
            if(paddr >= pageTables[i] && paddr < pageTables[i] + PAGE_SIZE) {
              found = 1;
              paddr += PAGE_SIZE;
              break;
            }
          }

          if(found)
            continue;

          // Is this page reserved?

          if(isReservedPage(paddr, info, 0)) {
            paddr += PAGE_SIZE;
            continue;
          }

          // Read each PTE that maps to the page stack and compare with pte.base, if a match, mark it as found

          for(addr_t addr = ((addr_t)freePageStack);
              addr < (((addr_t)freePageStack) + (size_t)pageStackSize); addr +=
              PAGE_SIZE)
          {
            pde_t pde = readPDE(PDE_INDEX(addr), CURRENT_ROOT_PMAP);

            if(!pde.isPresent)
              stopInit(
                  "Error while initializing memory: PDE doesn't map to a page table.");

            pte_t pte = readPTE(PTE_INDEX(addr), PDE_BASE(pde));

            if(!pte.isPresent)
              stopInit(
                  "Error while initializing memory: PTE doesn't map to a page.");

            if((paddr_t)PFRAME_TO_ADDR(pte.base) == paddr) {
              found = 1;
              paddr += PAGE_SIZE;
              break;
            }
          }

          if(found)
            continue;
          else {
            *freePageStackTop = (addr_t)paddr;
            freePageStackTop++;
            lastKernelFreePage = (addr_t)paddr;
            paddr += PAGE_SIZE;
            pagesAdded++;
          }
        }
      }
    }
    kprintf("Page allocator initialized. Added %d pages to free stack.\n",
            pagesAdded);
  }
  else {
    kprintf("Multiboot memory map is missing.\n");
    assert(false);
  }
}
/*
 int memcmp(const char *s1, const char *s2, register size_t n)
 {
 for( ; n && *s1 == *s2; n--, s1++, s2++);

 if( !n )
 return 0;
 else
 return (*s1 > *s2 ? 1 : -1);
 }
 */

int strncmp(const char *str1, const char *str2, size_t num) {
  register size_t i;

  if(!str1 && !str2)
    return 0;
  else if(!str1)
    return -1;
  else if(!str2)
    return 1;

  for(i = 0; i < num && str1[i] == str2[i] && str1[i]; i++)
    ;

  if(i == num)
    return 0;
  else
    return (str1[i] > str2[i] ? 1 : -1);
}

char* strstr(char *str, const char *substr) {
  register size_t i;

  if(str && substr) {
    for(; *str; str++) {
      for(i = 0; str[i] == substr[i] && substr[i]; i++)
        ;

      if(!substr[i])
        return str;
    }
  }

  return NULL;
}
/*
 size_t strlen(const char *s)
 {
 if( !s )
 return 0;

 return (size_t)(strchr(s, '\0') - s);
 }
 */
char* strchr(char *s, int c) {
  if(s) {
    while(*s) {
      if(*s == c)
        return (char*)s;
      else
        s++;
    }

    return c ? NULL : s;
  }

  return NULL;
}

/*
 void initPIC( void )
 {
 // Send ICW1 (cascade, edge-triggered, ICW4 needed)
 outByte( (uint16_t)0x20, 0x11 );
 outByte( (uint16_t)0xA0, 0x11 );
 ioWait();

 // Send ICW2 (Set interrupt vector)
 outByte( (uint16_t)0x21, IRQ(0) );
 outByte( (uint16_t)0xA1, IRQ(8) );
 ioWait();

 // Send ICW3 (IRQ2 input has a slave)
 outByte( (uint16_t)0x21, 0x04 );

 // Send ICW3 (Slave id 0x02)
 outByte( (uint16_t)0xA1, 0x02 );
 ioWait();

 // Send ICW4 (Intel 8086 mode)
 outByte( (uint16_t)0x21, 0x01 );
 outByte( (uint16_t)0xA1, 0x01 );
 ioWait();

 // Send OCW1 (Set mask to 0xFF)

 outByte( (uint16_t)0x21, 0xFF );
 outByte( (uint16_t)0xA1, 0xFF );
 ioWait();


 kprintf("Enabling IRQ 2\n");
 enableIRQ( 2 );
 }
 */
/* Various call once functions */

void addIDTEntry(void (*f)(void), unsigned int entryNum, unsigned int dpl) {
  assert(entryNum < 256);

  idt_entry_t *newEntry = &kernelIDT[entryNum];

  newEntry->offsetLower = (uint16_t)((uint32_t)f & 0xFFFFu);
  newEntry->selector = KCODE_SEL;
  newEntry->_resd = 0;
  newEntry->gateType = I_INT;
  newEntry->isStorage = 0;
  newEntry->dpl = MIN(dpl, 3u);
  newEntry->isPresent = 1;
  newEntry->offsetUpper = (uint16_t)((uint32_t)f >> 16);
}

void loadIDT(void) {
  struct IdtPointer idtPointer = {
    .limit = KERNEL_IDT_LEN,
    .base = (uint32_t)kernelIDT
  };

  __asm__("lidt %0" :: "m"(idtPointer) : "memory");
}

void setupGDT(void) {
  gdt_entry_t *tssDescriptor = &kernelGDT[TSS_SEL / sizeof(gdt_entry_t)];
  struct GdtPointer gdtPointer = {
    .base = (uint32_t)kernelGDT,
    .limit = 6 * sizeof(gdt_entry_t)
  };

  tssDescriptor->base1 = (uint32_t)&tss & 0xFFFFu;
  tssDescriptor->base2 = (uint8_t)(((uint32_t)&tss >> 16) & 0xFFu);
  tssDescriptor->base3 = (uint8_t)(((uint32_t)&tss >> 24) & 0xFFu);
  tssDescriptor->limit1 = sizeof tss; // Size of TSS structure and IO Bitmap (in pages)
  tssDescriptor->limit2 = 0;

  tss.ss0 = KDATA_SEL;

  __asm__ __volatile__("lgdt %0" :: "m"(gdtPointer) : "memory");
}

void stopInit(const char *msg) {
  disableInt();
  kprintf("Init failed: %s\nSystem halted.", msg);

  while(1)
    __asm__("hlt");
}

int initMemory(multiboot_info_t *info) {
  unsigned int totalPhysMem = (info->mem_upper + 1024) * 1024;
  addr_t addr;

  kprintf("Total Memory: %#x. %d pages. ", totalPhysMem,
          totalPhysMem / PAGE_SIZE);

  if(totalPhysMem < (64 << 20))
    stopInit("Not enough memory. System must have at least 64 MiB of memory.");

  kprintf("Kernel AddrSpace: %#p\n", kPageDir);

  pde_t pde = readPDE(PDE_INDEX((addr_t)EXT_PTR(kCode)), CURRENT_ROOT_PMAP);

  if(!pde.isPresent)
    stopInit("PDE isn't marked as present.");

  // Mark kernel code pages as read-only

  for(addr = (addr_t)EXT_PTR(kCode); addr < (addr_t)EXT_PTR(kData); addr +=
  PAGE_SIZE)
  {
    pte_t pte = readPTE(PTE_INDEX(addr), PDE_BASE(pde));

    if(!pte.isPresent)
      stopInit("Unable to read PTE");

    pte.isReadWrite = 0;

    if(IS_ERROR(writePTE(PTE_INDEX(addr), pte, PDE_BASE(pde))))
      stopInit("Unable to write PTE");

    invalidatePage(addr);
  }

#ifdef DEBUG
  size_t tcbSize;

  tcbSize = (size_t)&kTcbTableSize;

  kprintf("TCB Table size: %d bytes\n", tcbSize);
#endif /* DEBUG */

  setupGDT();

  return 0;
}

/*
 void initTimer( void )
 {
 outByte( (uint16_t)TIMER_CTRL, (uint8_t)(C_SELECT0 | C_MODE3 | BIN_COUNTER | RWL_FORMAT3) );
 outByte( (uint16_t)TIMER0, (uint8_t)(( TIMER_FREQ / TIMER_QUANTA_HZ ) & 0xFF) );
 outByte( (uint16_t)TIMER0, (uint8_t)(( TIMER_FREQ / TIMER_QUANTA_HZ ) >> 8) );

 kprintf("Enabling IRQ 0\n");
 enableIRQ( 0 );
 }
 */

void initInterrupts(void) {
  for(unsigned int i = 0; i < 32; i++)
    addIDTEntry(cpuExHandlers[i], i, 0);

  for(unsigned int i = 0; i < NUM_IRQS; i++)
    addIDTEntry(irqIntHandlers[i], IRQ(i), 0);

  //initPIC();
  loadIDT();
}

bool isValidElfExe(elf_header_t *image) {
  return image && VALID_ELF(image)
         && image->identifier[EI_VERSION] == EV_CURRENT
         && image->type == ET_EXEC && image->machine == EM_386
         && image->version == EV_CURRENT
         && image->identifier[EI_CLASS] == ELFCLASS32;
}

tcb_t* loadElfExe(addr_t img, addr_t addrSpace, void *uStack) {
  elf_header_t image;
  elf_sheader_t sheader;
  tcb_t *thread;
  addr_t page;
  pde_t pde;
  pte_t pte;
  size_t i;
  size_t offset;

  peek(img, &image, sizeof image);
  pte.isPresent = 0;

  if(!isValidElfExe(&image)) {
    kprintf("Not a valid ELF executable.\n");
    return NULL;
  }

  thread = createThread((void*)image.entry, addrSpace, uStack);

  if(thread == NULL) {
    kprintf("loadElfExe(): Couldn't create thread.\n");
    return NULL;
  }

  /* Create the page table before mapping memory */

  for(i = 0; i < image.shnum; i++) {
    peek((img + image.shoff + i * image.shentsize), &sheader, sizeof sheader);

    if(!(sheader.flags & SHF_ALLOC))
      continue;

    for(offset = 0; offset < sheader.size; offset += PAGE_SIZE) {
      pde = readPDE(PDE_INDEX(sheader.addr + offset), addrSpace);

      if(!pde.isPresent) {
        page = allocPageFrame();
        clearPhysPage(page);

        pde.base = (uint32_t)ADDR_TO_PFRAME(page);
        pde.isReadWrite = 1;
        pde.isUser = 1;
        pde.isPresent = 1;

        if(IS_ERROR(writePDE(PDE_INDEX(sheader.addr + offset), pde, addrSpace)))
          RET_MSG(NULL, "loadElfExe(): Unable to write PDE");
      }
      else if(pde.isLargePage)
        RET_MSG(
            NULL,
            "loadElfExe(): Memory region has already been mapped to a large page.");

      pte = readPTE(PTE_INDEX(sheader.addr + offset), PDE_BASE(pde));

      if(!pte.isPresent) {
        pte.isUser = 1;
        pte.isReadWrite = IS_FLAG_SET(sheader.flags, SHF_WRITE);
        pte.isPresent = 1;

        if(sheader.type == SHT_PROGBITS)
          pte.base = (uint32_t)ADDR_TO_PFRAME(
              (uint32_t )img + sheader.offset + offset);
        else if(sheader.type == SHT_NOBITS) {
          page = allocPageFrame();

          if(page == NULL_PADDR)
            stopInit("loadElfExe(): No more physical pages are available.");

          clearPhysPage(page);
          pte.base = (uint32_t)ADDR_TO_PFRAME(page);
        }
        else
          continue;

        if(IS_ERROR(
            writePTE(PTE_INDEX(sheader.addr + offset), pte, PDE_BASE(pde))))
          RET_MSG(NULL, "loadElfExe(): Unable to write PDE");
      }
      else if(sheader.type == SHT_NOBITS)
        memset((void*)(sheader.addr + offset), 0,
        PAGE_SIZE - (offset % PAGE_SIZE));
    }
  }

  return thread;
}

struct InitStackArgs {
  uint32_t returnAddress;
  multiboot_info_t *multibootInfo;
  addr_t firstFreePage;
  unsigned char code[4];
};

/**
 Bootstraps the initial server and passes necessary boot data to it.
 */

void bootstrapInitServer(multiboot_info_t *info) {
  addr_t initServerStack = (addr_t)INIT_SERVER_STACK_TOP;
  addr_t initServerPDir = INVALID_PFRAME;
  elf_header_t elf_header;

  /* code:

   xor    eax, eax
   xor    ebx, ebx
   inc    ebx
   int    0xFF     # sys_exit(1)
   nop
   nop
   nop
   ud2             # Trigger Invalid Opcode Exception: #UD
   */

  struct InitStackArgs stackData = {
    .returnAddress = initServerStack - sizeof((struct InitStackArgs*)0)->code,
    .multibootInfo = info,
    .firstFreePage = lastKernelFreePage + PAGE_SIZE,
    .code = {
      0x90,
      0x90,
      0x0F,
      0x0B
    }
  };

  kprintf("Bootstrapping initial server...\n");

  peek(initServerImg, &elf_header, sizeof elf_header);

  if(!isValidElfExe(&elf_header)) {
    kprintf("Invalid ELF exe\n");
    goto failedBootstrap;
  }

  if((initServerPDir = allocPageFrame()) == INVALID_PFRAME) {
    kprintf("Unable to create page directory for initial server.\n");
    goto failedBootstrap;
  }

  if(clearPhysPage(initServerPDir) != E_OK) {
    kprintf("Unable to clear init server page directory.\n");
    goto freeInitServerPDir;
  }

  pde_t pde;
  pte_t pte;

  clearMemory(&pde, sizeof(pde_t));
  clearMemory(&pte, sizeof(pte_t));

  addr_t stackPTab = allocPageFrame();

  if(stackPTab == INVALID_PFRAME) {
    kprintf("Unable to initialize stack page table\n");
    goto freeStackPTab;
  }
  else
    clearPhysPage(stackPTab);

  pde.base = (uint32_t)ADDR_TO_PFRAME(stackPTab);
  pde.isReadWrite = 1;
  pde.isUser = 1;
  pde.isPresent = 1;

  addr_t stackPage = allocPageFrame();

  if(stackPage == INVALID_PFRAME) {
    kprintf("Unable to initialize stack page.\n");
    goto freeStackPage;
  }

  pte.base = (uint32_t)ADDR_TO_PFRAME(stackPage);
  pte.isReadWrite = 1;
  pte.isUser = 1;
  pte.isPresent = 1;

  if(IS_ERROR(
      writePDE(PDE_INDEX(initServerStack-PAGE_SIZE), pde, initServerPDir))
     || IS_ERROR(writePTE(PTE_INDEX(initServerStack-PAGE_SIZE), pte, stackPTab)))
  {
    kprintf("Unable to write page map entries for init server stack.\n");
    goto freeStackPage;
  }

  if((initServerThread = loadElfExe(
      initServerImg, initServerPDir,
      (void*)(initServerStack - sizeof(stackData))))
     == NULL) {
    kprintf("Unable to load ELF executable.\n");
    goto freeStackPage;
  }

  poke(stackPage + PAGE_SIZE - sizeof(stackData), &stackData,
       sizeof(stackData));
  kprintf("Starting initial server... %#p\n", initServerThread);

  if(IS_ERROR(startThread(initServerThread)))
    goto releaseInitThread;

  return;

releaseInitThread:
  releaseThread(initServerThread);

freeStackPage:
  freePageFrame(stackPage);
freeStackPTab:
  freePageFrame(stackPTab);
freeInitServerPDir:
  freePageFrame(initServerPDir);
failedBootstrap:
  kprintf("Unable to start initial server.\n");
  return;
}

#ifdef DEBUG
static void DISC_CODE(showCPU_Features(void));
static void DISC_CODE(showMBInfoFlags(multiboot_info_t *));

union ProcessorVersion {
  struct {
    uint32_t steppingId :4;
    uint32_t model :4;
    uint32_t familyId :4;
    uint32_t processorType :2;
    uint32_t _resd :2;
    uint32_t extModelId :4;
    uint32_t extFamilyId :8;
    uint32_t _resd2 :4;
  };
  uint32_t value;
};

union AdditionalCpuidInfo {
  struct {
    uint8_t brandIndex;
    uint8_t cflushSize;
    uint8_t maxLogicalProcessors;
    uint8_t lapicId;
  };
  uint32_t value;
};

void showMBInfoFlags(multiboot_info_t *info) {
  const char *names[] = {
    "MEM",
    "BOOT_DEV",
    "CMDLINE",
    "MODS",
    "SYMTAB",
    "SHDR",
    "MMAP",
    "DRIVES",
    "CONFIG",
    "BOOTLDR",
    "APM_TAB",
    "GFX_TAB"
  };

  kprintf("Mulitboot Information Flags:\n");

  for(size_t i = 0; i < 12; i++) {
    if(IS_FLAG_SET(info->flags, (1u << i)))
      kprintf("%s\n", names[i]);
  }
}

void showCPU_Features(void) {
  union ProcessorVersion procVersion;
  union AdditionalCpuidInfo additionalInfo;
  uint32_t features2;
  uint32_t features;

  const char *featuresStr[32] = {
    "fpu",
    "vme",
    "de",
    "pse",
    "tsc",
    "msr",
    "pae",
    "mce",
    "cx8",
    "apic",
    "???",
    "sep",
    "mtrr",
    "pge",
    "mca",
    "cmov",
    "pat",
    "pse-36",
    "psn",
    "clfsh",
    "???",
    "ds",
    "acpi",
    "mmx",
    "fxsr",
    "sse",
    "sse2",
    "ss",
    "htt",
    "tm",
    "ia64",
    "pbe"
  };

  const char *features2Str[32] = {
    "sse",
    "pclmulqdq",
    "dtes64",
    "monitor",
    "ds-cpl",
    "vmx",
    "smx",
    "est",
    "tm2",
    "ssse3",
    "cntx-id",
    "sdbg",
    "fma",
    "cx16",
    "xtpr",
    "pdcm",
    "???",
    "pcid",
    "dca",
    "sse4.1",
    "sse4.2",
    "x2apic",
    "movbe",
    "popcnt",
    "tsc-deadline",
    "aes",
    "xsave",
    "osxsave",
    "avx",
    "f16c",
    "rdrnd",
    "hypervisor"
  };

  const char *processorType[4] = {
    "OEM Processor",
    "Intel Overdrive Processor",
    "Dual Processor",
    "???"
  };

  __cpuid(1, procVersion.value, additionalInfo.value, features2, features);

  kprintf("CPUID Features:\n");

  kprintf("%s\n", processorType[procVersion.processorType]);

  kprintf(
      "Model: %d\n",
      procVersion.model + (
          (procVersion.familyId == 6 || procVersion.familyId == 15) ?
              (procVersion.extModelId << 4) : 0));

  kprintf("Family: %d\n", procVersion.familyId + (procVersion.familyId == 15 ? procVersion.extFamilyId : 0));
  kprintf("Stepping: %d\n\n", procVersion.steppingId);

  kprintf("Brand Index: %d\n", additionalInfo.brandIndex);

  if(IS_FLAG_SET(features, 1 << 19))
    kprintf("Cache Line Size: %d bytes\n", 8 * additionalInfo.cflushSize);

  if(IS_FLAG_SET(features, 1 << 28)) {
    size_t numIds;

    if(additionalInfo.maxLogicalProcessors) {
      uint32_t msb = _bit_scan_reverse(additionalInfo.maxLogicalProcessors);

      if(additionalInfo.maxLogicalProcessors & ((1 << msb) - 1))
        numIds = (1 << (msb + 1));
      else
        numIds = 1 << msb;
    }
    else
      numIds = 0;

    kprintf("Unique APIC IDs per physical processor: %d\n", numIds);
  }
  kprintf("Local APIC ID: 0x%02x\n\n", additionalInfo.lapicId);

  kprintf("Processor Features:");

  for(size_t i = 0; i < 32; i++) {
    if(IS_FLAG_SET(features, 1 << i))
      kprintf(" %s", featuresStr[i]);
  }

  for(size_t i = 0; i < 32; i++) {
    if(IS_FLAG_SET(features2, 1 << i))
      kprintf(" %s", features2Str[i]);
  }

  kprintf("\n");
}
#endif /* DEBUG */

void initStructures(multiboot_info_t *info) {
  kprintf("Initializing free page allocator.\n");
  initPageAllocator(info);

  clearMemory(tcbTable, (size_t)&kTcbTableSize);
}

void initPaging(void) {
  clearMemory(kPageDir, PAGE_SIZE);
  clearMemory(kPageTab, PAGE_SIZE);
  clearMemory(kMapAreaPTab, PAGE_SIZE);

  for(size_t i = 0; i < PTE_INDEX(EXTENDED_MEMORY); i++) {
    pte_t *pte = &kPageTab[i];

    if(i < PTE_INDEX(BIOS_ROM)) {
      pte->isReadWrite = 1;

      if(i >= PTE_INDEX(VGA_RAM)) {
        pte->pcd = 1;
        pte->pwt = 1;
      }
    }

    pte->base = (uint32_t)i;
    pte->isPresent = 1;
  }

  pde_t *pde = &kPageDir[0].pde;

  pde->base = PADDR_TO_PDE_BASE(KVIRT_TO_PHYS(kPageTab));
  pde->isPresent = 1;
  pde->isReadWrite = 1;

  /* Map lower 2 GiB of physical memory to kernel space.
   * The kernel has to be careful not to write to read-only or non-existent areas.
   */

  paddr_t physAddr = 0;

  for(size_t pdeIndex = PDE_INDEX(KERNEL_VSTART);
      pdeIndex < LARGE_PDE_ENTRY_COUNT; pdeIndex++, physAddr += LARGE_PAGE_SIZE)
  {
    large_pde_t *largePde = &kPageDir[pdeIndex].largePde;

    setLargePdeBase(largePde, (paddr_t)NDX_TO_VADDR(physAddr, 0, 0));

    if(pdeIndex < PDE_INDEX(KERNEL_VEND))
      largePde->global = 1;

    largePde->isLargePage = 1;
    largePde->isReadWrite = 1;
    largePde->isPresent = 1;
  }

  // Set the page directory
  setCR3((uint32_t)KVIRT_TO_PHYS((addr_t )kPageDir));

  // Enable debugging extensions, large pages, global pages, FXSAVE/FXRSTOR, and SIMD exceptions
  setCR4(CR4_DE | CR4_PSE | CR4_PGE | CR4_OSFXSR | CR4_OSXMMEXCPT);

  // Enable paging
  setCR0(getCR0() | CR0_PG);

  // Reload segment registers

  setDs(KDATA_SEL);
  setEs(KDATA_SEL);
  setFs(KDATA_SEL);
  setGs(KDATA_SEL);
  setSs(KDATA_SEL);
  setCs(KCODE_SEL);
}

/**
 Bootstraps the kernel.

 @param info The multiboot structure passed by the bootloader.
 */

void init(multiboot_info_t *info) {
  setCR0(CR0_PE | CR0_MP | CR0_WP);

  // Paging hasn't been initialized yet.

  initPaging();

  info = (multiboot_info_t*)KPHYS_TO_VIRT(info);
  //  memory_map_t *mmap;
  module_t *module;
  bool initServerFound = false;
  char *initServerStrPtr = NULL;
  char *initServerStrEnd = NULL;

#ifdef DEBUG

#endif /* DEBUG */

#ifdef DEBUG
  init_serial();
  initVideo();
  clearScreen();
  showMBInfoFlags(info);
  showCPU_Features();
#endif

  initServerStrPtr = strstr((char*)KPHYS_TO_VIRT(info->cmdline),
  INIT_SERVER_FLAG);

  /* Locate the initial server string (if it exists) */

  if(initServerStrPtr) {
    initServerStrPtr += (sizeof( INIT_SERVER_FLAG) - 1);
    initServerStrEnd = strchr(initServerStrPtr, ' ');

    if(!initServerStrEnd)
      initServerStrEnd = strchr(initServerStrPtr, '\0');
  }
  else {
    kprintf("Initial server not specified.\n");
    stopInit("No boot modules found.\nNo initial server to start.");
  }

  if(IS_FLAG_SET(info->flags, MBI_FLAGS_MEM))
    kprintf("Lower Memory: %d B Upper Memory: %d B\n", info->mem_lower << 10,
            info->mem_upper << 10);

  kprintf("Boot info struct: %#p\nModules located at %#p. %d modules\n", info,
          info->mods_addr, info->mods_count);

  module = (module_t*)KPHYS_TO_VIRT(info->mods_addr);

  /* Locate the initial server module. */

  for(size_t i = info->mods_count; i; i--, module++) {
    if(strncmp((char*)KPHYS_TO_VIRT(module->string), initServerStrPtr,
               initServerStrEnd - initServerStrPtr)
       == 0) {
      kprintf("Found image at %#p", (void*)module->mod_start);
      initServerImg = (addr_t)module->mod_start;
      initServerFound = true;
      break;
    }
  }

  if(!initServerFound)
    stopInit("Can't find initial server.");

  /* Initialize memory */

  if(initMemory(info) < 0)
    stopInit("Not enough memory! At least 8 MiB is needed.");

  kprintf("Initializing interrupt handling.\n");
  initInterrupts();

  kprintf("Initializing data structures.\n");
  initStructures(info);

  /*
   struct RSDPointer rsdp;
   struct MP_FloatingHeader mpHeader;
   paddr_t rsdpAddr;
   paddr_t mpHeaderAddr;

   if((rsdpAddr=findRSDP(&rsdp)))
   {
   kprintf("%.8s found at %#llx. RSDT at %#x\n", rsdp.signature, rsdpAddr, rsdp.rsdtAddress);

   struct ACPI_DT_Header rsdt;

   if(rsdp.rsdtAddress && peek((paddr_t)rsdp.rsdtAddress, &rsdt, sizeof rsdt) == E_OK
   && isValidAcpiHeader((paddr_t)rsdp.rsdtAddress))
   {
   uint32_t rsdtEntries[64];
   size_t entriesLeft = (rsdt.length - sizeof(struct ACPI_DT_Header)) / sizeof(uint32_t);
   size_t entriesRead = 0;

   kprintf("%.4s found at %#x\n", rsdt.signature, rsdp.rsdtAddress);

   while(entriesLeft)
   {
   size_t entriesPeeked =  MIN(entriesLeft, sizeof rsdtEntries / sizeof(uint32_t));

   if(peek((paddr_t)(rsdp.rsdtAddress + sizeof (struct ACPI_DT_Header) + entriesRead * sizeof(uint32_t)),
   rsdtEntries,  entriesPeeked * sizeof(uint32_t)) == E_OK)
   {
   for(size_t i=0; i < entriesPeeked; i++)
   {
   struct ACPI_DT_Header header;

   if(peek((paddr_t)rsdtEntries[i], &header, sizeof header) == E_OK
   && isValidAcpiHeader((paddr_t)rsdtEntries[i]))
   {
   kprintf("%.4s found at %#x\n", header.signature, rsdtEntries[i]);
   }
   }
   }
   else
   kprintf("Unable to read RSDT entries at %#x\n",
   (rsdp.rsdtAddress + sizeof (struct ACPI_DT_Header) + entriesRead * sizeof(uint32_t)));

   entriesLeft -= entriesPeeked;
   entriesRead += entriesPeeked;
   }
   }
   else
   kprintf("Unable to read RSDT\n");
   }
   else
   kprintf("RSDP not found\n");

   if((mpHeaderAddr=findMP_Header(&mpHeader)))
   kprintf("MP Floating header found at %#llx. Table at %#x.\n", mpHeaderAddr, mpHeader.tableAddress);
   else
   kprintf("MP Floating header not found.\n");
   */
  //  enable_apic();
  //  init_apic_timer();
  /*
   kprintf("Initializing timer.\n");
   initTimer();

   kprintf("Initializing IRQ threads.\n");

   for(int irq=0, tid=IRQ0_TID; irq <= 15; irq++, tid++)
   {
   tcb_t *newThread;
   struct ExecutionState irqExecState;

   uint32_t *stackTop = (uint32_t *)allocateKernelStack();
   *--stackTop = 0x90900B0F; // instructions: ud2 nop nop
   *--stackTop = irq;
   --stackTop;
   *stackTop = (uint32_t)(stackTop+2);

   newThread = createKernelThread(tid,
   (irq == 0 || irq == 7) ? ignoreIrqThreadMain : irqThreadMain,
   stackTop, &irqExecState);

   // todo: irqExecState must be saved somewhere

   if(!newThread)
   kprintf("Unable to create thread with tid: %d", tid);
   }
   */

  bootstrapInitServer(info);

  kprintf("\n%#x bytes of discardable code.",
          (addr_t)EXT_PTR(kdData) - (addr_t)EXT_PTR(kdCode));
  kprintf(" %#x bytes of discardable data.\n",
          (addr_t)EXT_PTR(kBss) - (addr_t)EXT_PTR(kdData));
  kprintf("Discarding %d bytes in total\n",
          (addr_t)EXT_PTR(kBss) - (addr_t)EXT_PTR(kdCode));

  /* Release the pages for the code and data that will never be used again. */

  for(addr_t addr = (addr_t)EXT_PTR(kdCode); addr < (addr_t)EXT_PTR(kBss);) {
    pde_t pde = readPDE(PDE_INDEX(addr), CURRENT_ROOT_PMAP);

    if(!pde.isPresent) {
      addr = ALIGN_DOWN(addr, PAGE_TABLE_SIZE) + PAGE_TABLE_SIZE;
      continue;
    }
    else {
      pte_t pte = readPTE(PTE_INDEX(addr), PDE_BASE(pde));

      if(pte.isPresent) {
        freePageFrame(PTE_BASE(pte));
        addr += PAGE_SIZE;
      }
    }
  }

  // Finally, unmap any unneeded mapped pages in the kernel's address space

  pde_t pde = readPDE(PDE_INDEX(0), CURRENT_ROOT_PMAP);

  if(pde.isPresent) {
    if(pde.isLargePage) {
      pde.isPresent = 0;

      if(IS_ERROR(writePDE(PDE_INDEX(0), pde, CURRENT_ROOT_PMAP)))
        stopInit("Error discarding pages. Unable to write PDE.");
    }
    else {
      for(addr_t addr = (addr_t)0; addr < (addr_t)PAGE_TABLE_SIZE; addr +=
      PAGE_SIZE)
      {
#ifdef DEBUG

        if(addr == VGA_COLOR_TEXT) {
          addr = (addr_t)ISA_EXT_ROM;
          continue;
        }
#endif  /* DEBUG */

        pte_t pte = readPTE(PTE_INDEX(addr), PDE_BASE(pde));

        if(pte.isPresent) {
          pte.isPresent = 0;

          if(IS_ERROR(writePTE(PTE_INDEX(addr), pte, PDE_BASE(pde))))
            stopInit("Error discarding pages. Unable to write PTE.");

          invalidatePage(addr);
        }
      }
    }
  }

  // Set the single kernel stack that will be shared between threads

  tss.esp0 = (uint32_t)kernelStackTop;

  // Initialize FPU to a known state
  __asm__("fninit\n");

  // Set MSRs to enable sysenter/sysexit functionality

  __asm__ __volatile__(
      "mov %0, %%ecx\n"
      "xor %%edx, %%edx\n"
      "mov %1, %%eax\n"
      "wrmsr\n"

      "mov %2, %%ecx\n"
      "xor %%edx, %%edx\n"
      "mov %3, %%eax\n"
      "wrmsr\n"

      "mov %4, %%ecx\n"
      "xor %%edx, %%edx\n"
      "mov %5, %%eax\n"
      "wrmsr\n"
      :: "i"(SYSENTER_CS_MSR), "r"(KCODE_SEL), "i"(SYSENTER_ESP_MSR), "r"(kernelStackTop),
      "i"(SYSENTER_EIP_MSR), "r"(sysenter));

  kprintf("Context switching...\n");
  while(1)
    ;

  enterContext(initServerThread->rootPageMap, initServerThread->userExecState);
  stopInit("Error: Context switch failed.");
}
