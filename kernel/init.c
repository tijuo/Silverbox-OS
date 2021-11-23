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
#include <kernel/lowlevel.h>
#include <cpuid.h>
#include <x86intrin.h>
#include <limits.h>
#include <kernel/apic.h>
#include <kernel/pic.h>
#include <os/io.h>

#define KERNEL_IDT_LEN	(64 * sizeof(struct IdtEntry))

#define RSDP_SIGNATURE  "RSD PTR "
#define PARAGRAPH_LEN   16

#define DISC_CODE SECTION(".dtext")
#define DISC_DATA SECTION(".ddata")

#define EBDA                0x80000u
#define VGA_RAM             0xA0000u
#define VGA_COLOR_TEXT      0xB8000u
#define ISA_EXT_ROM         0xC0000u
#define BIOS_EXT_ROM        0xE0000u
#define BIOS_ROM            0xF0000u
#define EXTENDED_MEMORY     0x100000u

#define INIT_SERVER_FLAG  "--init"

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
};

_Static_assert(sizeof(struct RSDPointer) == 36, "RSDPointer struct should be 36 bytes.");

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

_Static_assert(sizeof(struct ACPI_DT_Header) == 36, "ACPI_DT_Header should be 36 bytes.");

struct MADT_Header {
  struct ACPI_DT_Header dtHeader;
  uint32_t lapicAddress;
  uint32_t flags;
};

_Static_assert(sizeof(struct MADT_Header) == 44, "MADT_Header should be 44 bytes.");

struct IC_Header {
  uint8_t type;
  uint8_t length;
};

#define TYPE_PROC_LAPIC     0
#define TYPE_IOAPIC         1

struct ProcessorLAPIC_Header {
  struct IC_Header header;
  uint8_t uid;
  uint8_t lapicId;
  uint32_t flags;
};

#define PROC_LAPIC_ENABLED    1
#define PROC_LAPIC_ONLINE     2

struct IOAPIC_Header {
  struct IC_Header header;
  uint8_t ioapicId;
  uint8_t _resd;
  uint32_t ioapicAddress;
  uint32_t globalSysIntBase;
};

DISC_CODE static void disableIRQ(unsigned int irq);

DISC_CODE static bool isValidAcpiHeader(uint64_t physAddress);
DISC_CODE void readAcpiTables(void);
NON_NULL_PARAMS DISC_CODE addr_t findRSDP(struct RSDPointer *header);

extern pte_t kMapAreaPTab[PTE_ENTRY_COUNT];

DISC_DATA multiboot_info_t *multibootInfo;
DISC_DATA ALIGNED(PAGE_SIZE) uint8_t kBootStack[4 * PAGE_SIZE];
DISC_DATA void *kBootStackTop = kBootStack + sizeof kBootStack;
DISC_DATA ALIGNED(PAGE_SIZE) pmap_entry_t kPageDir[PMAP_ENTRY_COUNT]; // The initial page directory used by the kernel on bootstrap

extern gdt_entry_t kernelGDT[8];
extern idt_entry_t kernelIDT[NUM_EXCEPTIONS + NUM_IRQS];

extern tcb_t *initServerThread;
extern uint8_t *kernelStackTop;

DISC_CODE void initPaging(void);
DISC_CODE static tcb_t* loadElfExe(addr_t, uint32_t, void*);
DISC_CODE static bool isValidElfExe(elf_header_t *image);
DISC_CODE static void initInterrupts(void);
//DISC_CODE static void initTimer( void ));
DISC_CODE static int initMemory(multiboot_info_t *info);
DISC_CODE static void setupGDT(void);
DISC_CODE static void stopInit(const char*);
DISC_CODE static void bootstrapInitServer(multiboot_info_t *info);
DISC_CODE void init(multiboot_info_t*);
//DISC_CODE static void initPIC( void ));
DISC_CODE static int memcmp(const void *m1, const void *m2, size_t n);
DISC_CODE static int strncmp(const char*, const char*, size_t num);
DISC_CODE static size_t strlen(const char *s);
DISC_CODE static char* strstr(const char*, const char*);
DISC_CODE static char* strchr(const char*, int);
DISC_CODE void addIDTEntry(void (*f)(void), unsigned int entryNum,
                           unsigned int dpl);
DISC_CODE void loadIDT(void);
DISC_CODE addr_t allocPageFrame(void);

// Assumes 0xE0000 - 0x100000 has been 1:1 mapped
addr_t findRSDP(struct RSDPointer *rsdPointer) {
  addr_t retPtr = (addr_t)NULL;

  for(uint8_t *ptr = (uint8_t*)KPHYS_TO_VIRT(BIOS_EXT_ROM);
      ptr < (uint8_t*)KPHYS_TO_VIRT(EXTENDED_MEMORY); ptr += PARAGRAPH_LEN)
  {
    if(memcmp(ptr, RSDP_SIGNATURE, sizeof rsdPointer->signature) == 0) {
      uint8_t checksum = 0;

      for(size_t i = 0; i < offsetof(struct RSDPointer, length); i++)
        checksum += ptr[i];

      if(checksum == 0) {
        memcpy(rsdPointer, ptr, sizeof *rsdPointer);
        retPtr = (addr_t)ptr;
        break;
      }
    }
  }

  return retPtr;
}

bool isValidAcpiHeader(uint64_t physAddress) {
  struct ACPI_DT_Header header;
  uint8_t checksum = 0;
  uint8_t buffer[128];
  size_t bytesRead = 0;
  size_t headerLen;

  if(physAddress >= MAX_PHYS_MEMORY)
    RET_MSG(false, "Physical memory is out of range.");

  if(IS_ERROR(peek(physAddress, &header, sizeof header)))
    RET_MSG(false, "Unable to read ACPI descriptor table header.");

  headerLen = header.length;

  memcpy(buffer, &header, sizeof header);

  for(size_t i = 0; i < sizeof header; i++, bytesRead++)
    checksum += buffer[i];

  while(bytesRead < headerLen) {
    size_t bytesToRead = MIN(sizeof buffer, headerLen - bytesRead);

    if(IS_ERROR(peek(physAddress + bytesRead, buffer, bytesToRead)))
      RET_MSG(false, "Unable to read ACPI descriptor table header.");

    for(size_t i = 0; i < bytesToRead; i++, bytesRead++)
      checksum += buffer[i];
  }

  return checksum == 0 ? true : false;
}

void readAcpiTables(void) {
  struct RSDPointer rsdp;
  addr_t rsdpAddr;

  // Look for the RSDP

  for(uint8_t *ptr = (uint8_t*)KPHYS_TO_VIRT(BIOS_EXT_ROM);
      ptr < (uint8_t*)KPHYS_TO_VIRT(EXTENDED_MEMORY); ptr += PARAGRAPH_LEN)
  {
    if(memcmp(ptr, RSDP_SIGNATURE, sizeof rsdp.signature) == 0) {
      uint8_t checksum = 0;

      for(size_t i = 0; i < offsetof(struct RSDPointer, length); i++)
        checksum += ptr[i];

      if(checksum == 0) {
        rsdp = *(struct RSDPointer*)ptr;
        rsdpAddr = (addr_t)ptr;
        break;
      }
    }
  }

  if(rsdpAddr) {
    // Now go through the RSDT, looking for the MADT

    kprintf("%.8s found at %#x. RSDT at %#x\n", rsdp.signature, rsdpAddr,
            rsdp.rsdtAddress);

    if(rsdp.rsdtAddress && isValidAcpiHeader((uint64_t)rsdp.rsdtAddress)) {
      struct ACPI_DT_Header rsdt;

      if(IS_ERROR(peek((uint64_t )rsdp.rsdtAddress, &rsdt, sizeof rsdt))) {
        kprintf("Unable to read RSDT\n");
        return;
      }

      size_t totalEntries = (rsdt.length - sizeof(struct ACPI_DT_Header))
          / sizeof(uint32_t);

      kprintf("%.4s found at %#x\n", rsdt.signature, rsdp.rsdtAddress);

      for(size_t rsdtIndex = 0; rsdtIndex < totalEntries; rsdtIndex++) {
        uint32_t rsdtEntryPtr;

        if(IS_ERROR(
            peek(
                (uint64_t )rsdp.rsdtAddress + sizeof rsdt
                + rsdtIndex * sizeof(uint32_t),
                &rsdtEntryPtr, sizeof(uint32_t)))) {
          kprintf("Unable to read descriptor table.\n");
          return;
        }

        if(isValidAcpiHeader((uint64_t)rsdtEntryPtr)) {
          struct ACPI_DT_Header header;

          if(IS_ERROR(peek((uint64_t )rsdtEntryPtr, &header, sizeof header))) {
            kprintf("Unable to read descriptor table header.\n");
            return;
          }

          kprintf("%.4s found at %#x\n", header.signature, rsdtEntryPtr);

          // If the MADT is in the RSDT, then retrieve apic data

          if(memcmp(&header.signature, "APIC", 4) == 0) {
            struct MADT_Header madtHeader;

            if(IS_ERROR(
                peek((uint64_t )rsdtEntryPtr, &madtHeader, sizeof madtHeader)))
            {
              kprintf("Unable to read MADT header.\n");
              return;
            }

            kprintf("Local APIC address: %#x\n", madtHeader.lapicAddress);
            lapicPtr = madtHeader.lapicAddress;

            for(size_t madtOffset = sizeof madtHeader;
                madtOffset < madtHeader.dtHeader.length;)
            {
              struct IC_Header icHeader;

              if(IS_ERROR(
                  peek((uint64_t )rsdtEntryPtr + madtOffset, &icHeader,
                       sizeof icHeader))) {
                kprintf("Unable to read interrupt controller header.\n");
                return;
              }

              switch(icHeader.type) {
                case TYPE_PROC_LAPIC: {
                  /* Each processor has a local APIC. Use this to find each processor's
                   local APIC id. */
                  struct ProcessorLAPIC_Header procLapicHeader;

                  if(IS_ERROR(
                      peek((uint64_t )rsdtEntryPtr + madtOffset,
                           &procLapicHeader, sizeof procLapicHeader))) {
                    kprintf("Unable to read processor local APIC header.\n");
                    return;
                  }

                  if(IS_FLAG_SET(procLapicHeader.flags, PROC_LAPIC_ENABLED) || IS_FLAG_SET(
                      procLapicHeader.flags, PROC_LAPIC_ONLINE)) {
                    kprintf(
                        "Processor %d has local APIC id: %d%s\n",
                        procLapicHeader.uid,
                        procLapicHeader.lapicId,
                        IS_FLAG_SET(procLapicHeader.flags, PROC_LAPIC_ENABLED) ?
                            "" :
                        IS_FLAG_SET(procLapicHeader.flags, PROC_LAPIC_ONLINE) ?
                            " (offline)" : " (disabled)");
                    processors[numProcessors].lapicId = procLapicHeader.lapicId;
                    processors[numProcessors].acpiUid = procLapicHeader.uid;
                    processors[numProcessors].isOnline = !!IS_FLAG_SET(
                        procLapicHeader.flags, PROC_LAPIC_ENABLED);

                    numProcessors++;
                  }
                  break;
                }
                case TYPE_IOAPIC: {
                  struct IOAPIC_Header ioapicHeader;

                  if(IS_ERROR(
                      peek((uint64_t )rsdtEntryPtr + madtOffset, &ioapicHeader,
                           sizeof ioapicHeader))) {
                    kprintf("Unable to read processor local APIC header.\n");
                    return;
                  }

                  kprintf(
                      "IOAPIC id %d is at %#x. Global System Interrupt Base: %#x\n",
                      ioapicHeader.ioapicId, ioapicHeader.ioapicAddress,
                      ioapicHeader.globalSysIntBase);
                  ioapicPtr = ioapicHeader.ioapicId;
                  break;
                }
                default:
                  kprintf("APIC Entry type %d found.\n", icHeader.type);
                  break;
              }

              madtOffset += icHeader.length;
            }

            kprintf("%d processors found.\n", numProcessors);
          }
        }
        else
          kprintf("Unable to read RSDT entries at %#x\n", rsdtEntryPtr);
      }
    }
    else
      kprintf("Unable to read RSDT\n");
  }
  else
    kprintf("RSDP not found\n");
}

/*
 static unsigned DISC_CODE bcd2bin(unsigned num));
 static unsigned long long DISC_CODE mktime(unsigned int year, unsigned int month, unsigned int day, unsigned int hour,
 unsigned int minute, unsigned int second));
 */
DISC_DATA static addr_t initServerImg;
DISC_CODE static bool isReservedPage(uint64_t addr, multiboot_info_t *info,
                                     int isLargePage);

DISC_CODE int clearPhysPage(uint64_t phys);

DISC_DATA static addr_t firstFreePage;

DISC_DATA static void (*cpuExHandlers[NUM_EXCEPTIONS])(
    void) = {
      cpuEx0Handler, cpuEx1Handler, cpuEx2Handler, cpuEx3Handler, cpuEx4Handler, cpuEx5Handler,
      cpuEx6Handler, cpuEx7Handler, cpuEx8Handler, cpuEx9Handler, cpuEx10Handler, cpuEx11Handler,
      cpuEx12Handler, cpuEx13Handler, cpuEx14Handler, cpuEx15Handler, cpuEx16Handler, cpuEx17Handler,
      cpuEx18Handler, cpuEx19Handler, cpuEx20Handler, cpuEx21Handler, cpuEx22Handler, cpuEx23Handler,
      cpuEx24Handler, cpuEx25Handler, cpuEx26Handler, cpuEx27Handler, cpuEx28Handler, cpuEx29Handler,
      cpuEx30Handler, cpuEx31Handler
};

DISC_DATA static void (*irqIntHandlers[NUM_IRQS])(
    void) = {
      irq0Handler, irq1Handler, irq2Handler, irq3Handler, irq4Handler, irq5Handler,
      irq6Handler, irq7Handler, irq8Handler, irq9Handler, irq10Handler, irq11Handler,
      irq12Handler, irq13Handler, irq14Handler, irq15Handler, irq16Handler, irq17Handler,
      irq18Handler, irq19Handler, irq20Handler, irq21Handler, irq22Handler, irq23Handler
};

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
  unsigned int kernelLength = (unsigned int)&kSize;
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

NON_NULL_PARAMS int memcmp(const void *m1, const void *m2, size_t num) {
  size_t i;
  const char *mem1 = (const char*)m1;
  const char *mem2 = (const char*)m2;

  for(i = 0; i < num && mem1[i] == mem2[i]; i++)
    ;

  if(i == num)
    return 0;
  else
    return (mem1[i] > mem2[i] ? 1 : -1);
}

NON_NULL_PARAMS int strncmp(const char *str1, const char *str2, size_t num) {
  size_t i;

  for(i = 0; i < num && str1[i] == str2[i] && str1[i]; i++)
    ;

  if(i == num)
    return 0;
  else
    return (str1[i] > str2[i] ? 1 : -1);
}

char* strstr(const char *str, const char *substr) {
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

size_t strlen(const char *s)
{
 if( !s )
   return 0;

 return (size_t)(strchr(s, '\0') - s);
}

char* strchr(const char *s, int c) {
  if(s) {
    while(*s) {
      if(*s == c)
        return s;
      else
        s++;
    }

    return s;
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

void disableIRQ(unsigned int irq) {
  assert(irq < 16);

  // Send OCW1 (set IRQ mask)

  if(irq < PIC2_IRQ_START)
    outPort8( PIC1_PORT | 0x01,
             inPort8(PIC1_PORT | 0x01) | (uint8_t)FROM_FLAG_BIT(irq));
  else
    outPort8(
        PIC2_PORT | 0x01,
        inPort8(PIC2_PORT | 0x01) | (uint8_t)FROM_FLAG_BIT(irq-PIC2_IRQ_START));
}

void setupGDT(void) {
  gdt_entry_t *tssDescriptor = &kernelGDT[TSS_SEL / sizeof(gdt_entry_t)];
  struct GdtPointer gdtPointer = {
    .base = (uint32_t)kernelGDT,
    .limit = 6 * sizeof(gdt_entry_t)
  };

  size_t tssLimit = sizeof tss - 1;
  tssDescriptor->base1 = (uint32_t)&tss & 0xFFFFu;
  tssDescriptor->base2 = (uint8_t)(((uint32_t)&tss >> 16) & 0xFFu);
  tssDescriptor->base3 = (uint8_t)(((uint32_t)&tss >> 24) & 0xFFu);
  tssDescriptor->limit1 = (uint16_t)(tssLimit & 0xFFFF); // Size of TSS structure and IO Bitmap (in pages)
  tssDescriptor->limit2 = (uint8_t)(tssLimit >> 16);

  tss.ss0 = KDATA_SEL;
  tss.esp0 = (uint32_t)kernelStackTop;

  __asm__ __volatile__("lgdt %0" :: "m"(gdtPointer) : "memory");
  __asm__ __volatile__("ltr %%ax" :: "a"(TSS_SEL));
}

void stopInit(const char *msg) {
  disableInt();
  kprintf("Init failed: %s\nSystem halted.", msg);

  while(1)
    __asm__("hlt");
}

int initMemory(multiboot_info_t *info) {
  unsigned int totalPhysMem = (info->mem_upper + 1024) * 1024;

  kprintf("Total Memory: %#x. %d pages. ", totalPhysMem,
          totalPhysMem / PAGE_SIZE);

  if(totalPhysMem < (64 << 20))
    stopInit("Not enough memory. System must have at least 64 MiB of memory.");

  kprintf("Kernel AddrSpace: %#p\n", kPageDir);
  /*
   * No need to do this, since kernel memory is mapped with 4 MiB pages.
   * Discardable pages can be added to free page stack.
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
   */
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
  for(unsigned int i = 0; i < NUM_EXCEPTIONS; i++)
    addIDTEntry(cpuExHandlers[i], i, 0);

  for(unsigned int i = 0; i < NUM_IRQS; i++)
    addIDTEntry(irqIntHandlers[i], IRQ(i), 0);

  for(int i=0; i < 16; i++)
    disableIRQ(i);

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

        if(page == INVALID_PFRAME)
          stopInit("loadElfExec(): Unable to allocate PDE.");

        clearPhysPage(page);

        pde.base = (uint32_t)ADDR_TO_PFRAME(page);
        pde.isReadWrite = 1;
        pde.isUser = 1;
        pde.isPresent = 1;

        if(IS_ERROR(writePDE(PDE_INDEX(sheader.addr + offset), pde, addrSpace)))
          RET_MSG(NULL, "loadElfExe(): Unable to write PDE.");
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

          if(page == INVALID_PFRAME)
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
  size_t stackSize;
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
    .firstFreePage = firstFreePage,
    .stackSize = 0,
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
    goto failedBootstrap;
  }

  pde_t pde;
  pte_t pte;

  memset(&pde, 0, sizeof(pde_t));
  memset(&pte, 0, sizeof(pte_t));

  addr_t stackPTab = allocPageFrame();

  if(stackPTab == INVALID_PFRAME) {
    kprintf("Unable to initialize stack page table\n");
    goto failedBootstrap;
  }
  else
    clearPhysPage(stackPTab);

  pde.base = (uint32_t)ADDR_TO_PFRAME(stackPTab);
  pde.isReadWrite = 1;
  pde.isUser = 1;
  pde.isPresent = 1;

  if(IS_ERROR(
      writePDE(PDE_INDEX(initServerStack-PAGE_TABLE_SIZE), pde, initServerPDir)))
  {
    goto failedBootstrap;
  }

  for(size_t p = 0; p < (512 * 1024) / PAGE_SIZE; p++) {
    addr_t stackPage = allocPageFrame();

    if(stackPage == INVALID_PFRAME) {
      kprintf("Unable to initialize stack page.\n");
      goto failedBootstrap;
    }

    pte.base = (uint32_t)ADDR_TO_PFRAME(stackPage);
    pte.isReadWrite = 1;
    pte.isUser = 1;
    pte.isPresent = 1;

    if(IS_ERROR(
        writePTE(PTE_INDEX(initServerStack-(p+1)*PAGE_SIZE), pte, stackPTab))) {
      if(p == 0) {
        kprintf("Unable to write page map entries for init server stack.\n");
        goto failedBootstrap;
      }
      else
        break;
    }

    if(p == 0) {
      poke(stackPage + PAGE_SIZE - sizeof(stackData), &stackData,
           sizeof(stackData));
    }

    stackData.stackSize += PAGE_SIZE;
  }

  if((initServerThread = loadElfExe(
      initServerImg, initServerPDir,
      (void*)(initServerStack - sizeof(stackData))))
     == NULL) {
    kprintf("Unable to load ELF executable.\n");
    goto failedBootstrap;
  }

  kprintf("Starting initial server... %#p\n", initServerThread);

  if(IS_ERROR(startThread(initServerThread)))
    goto releaseInitThread;

  return;

releaseInitThread:
  releaseThread(initServerThread);

failedBootstrap:
  kprintf("Unable to start initial server.\n");
  return;
}

#ifdef DEBUG
DISC_CODE static void showCPU_Features(void);
DISC_CODE static void showMBInfoFlags(multiboot_info_t*);

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

  kprintf("Mulitboot Information Flags:\n\n");

  for(size_t i = 0; i < 12; i++) {
    if(IS_FLAG_SET(info->flags, (1u << i)))
      kprintf("%s\n", names[i]);
  }
  kprintf("\n");
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

  kprintf(
      "Family: %d\n",
      procVersion.familyId + (
          procVersion.familyId == 15 ? procVersion.extFamilyId : 0));
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
  kprintf("Local APIC ID: %#x\n\n", additionalInfo.lapicId);

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

void initPaging(void) {
  memset(kPageDir, 0, PAGE_SIZE);
  memset(kMapAreaPTab, 0, PAGE_SIZE);

  large_pde_t *pde = &kPageDir[0].largePde;

  setLargePdeBase(pde, 0);
  pde->isLargePage = 1;
  pde->isPresent = 1;
  pde->isReadWrite = 1;

  /* Map lower 256 MiB of physical memory to kernel space.
   * The kernel has to be careful not to write to read-only or non-existent areas.
   */

  for(addr_t addr = ALIGN_DOWN(KERNEL_VSTART, LARGE_PAGE_SIZE);
      addr < KERNEL_VEND && addr >= ALIGN_DOWN(KERNEL_VSTART, LARGE_PAGE_SIZE); addr +=
      LARGE_PAGE_SIZE)
  {
    size_t pdeIndex = PDE_INDEX(addr);
    large_pde_t *largePde = &kPageDir[pdeIndex].largePde;

    largePde->global = 1;
    setLargePdeBase(largePde, ADDR_TO_PFRAME(KVIRT_TO_PHYS(addr)));
    largePde->isLargePage = 1;

    largePde->isReadWrite = 1;
    largePde->isPresent = 1;
  }

  pde_t *kMapPde = &kPageDir[PDE_INDEX(KMAP_AREA2)].pde;

  kMapPde->base = ADDR_TO_PFRAME(KVIRT_TO_PHYS(kMapAreaPTab));

  kMapPde->isReadWrite = 1;
  kMapPde->isPresent = 1;

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

  // Paging hasn't been initialized yet. (Debugger may not work property until initPaging() is called.)

  initPaging();

  info = (multiboot_info_t*)KPHYS_TO_VIRT(info);
  multibootInfo = info;

  firstFreePage = KVIRT_TO_PHYS((addr_t )&kTcbEnd);

  //  memory_map_t *mmap;
  module_t *module;
  bool initServerFound = false;

#ifdef DEBUG

#endif /* DEBUG */

#ifdef DEBUG
  init_serial();
  initVideo();
  clearScreen();
  showMBInfoFlags(info);
  showCPU_Features();
#endif

  if(IS_FLAG_SET(info->flags, MBI_FLAGS_MEM))
    kprintf("Lower Memory: %d bytes Upper Memory: %d bytes\n",
            info->mem_lower << 10, info->mem_upper << 10);

  kprintf("Boot info struct: %#p\nModules located at %#p. %d modules\n", info,
          info->mods_addr, info->mods_count);

  module = (module_t*)KPHYS_TO_VIRT(info->mods_addr);

  /* Locate the initial server module. */

  for(size_t i = info->mods_count; i; i--, module++) {
    if(i == info->mods_count)
      initServerImg = (addr_t)module->mod_start;

    if(module->string) {
      const char *strPtr = (const char *)KPHYS_TO_VIRT(module->string);

      do {
        strPtr = (const char *)strstr(strPtr, INIT_SERVER_FLAG);
        const char *separator = strPtr + strlen(INIT_SERVER_FLAG);

        if(*separator == '\0' || (*separator >= '\t' && *separator <= '\r')) {
          initServerFound = true;
          initServerImg = (addr_t)module->mod_start;
          break;
        }
        else
          strPtr = separator;
      } while(strPtr);
    }
  }

  if(!initServerFound) {
    if(info->mods_count)
      kprintf("Initial server was not specified with --init. Using first module.\n");
    else
      stopInit("Can't find initial server.");
  }

  /* Initialize memory */

  if(initMemory(info) < 0)
    stopInit("Not enough memory! At least 8 MiB is needed.");

  kprintf("Initializing interrupt handling.\n");
  initInterrupts();

  readAcpiTables();

  if(numProcessors == 0)
    numProcessors = 1;

  //  enable_apic();
  //  init_apic_timer();
  /*
   kprintf("Initializing timer.\n");
   initTimer();
   */

  bootstrapInitServer(info);

  kprintf("\n%#x bytes of discardable code.",
          (addr_t)EXT_PTR(kdData) - (addr_t)EXT_PTR(kdCode));
  kprintf(" %#x bytes of discardable data.\n",
          (addr_t)EXT_PTR(kBss) - (addr_t)EXT_PTR(kdData));
  kprintf("Discarding %d bytes in total\n",
          (addr_t)EXT_PTR(kBss) - (addr_t)EXT_PTR(kdCode));

  /* Release the pages for the code and data that will never be used again. */

  for(addr_t addr = (addr_t)EXT_PTR(kdCode); addr < (addr_t)EXT_PTR(kBss);
      addr += PAGE_SIZE)
  {
    // todo: Mark these pages as free for init server
  }

  // Set the single kernel stack that will be shared between threads

  // Initialize FPU to a known state
  __asm__("fninit\n"
      "fxsave %0\n" :: "m"(initServerThread->xsaveState));

  // Set MSRs to enable sysenter/sysexit functionality

  wrmsr(SYSENTER_CS_MSR, KCODE_SEL);
  wrmsr(SYSENTER_ESP_MSR, (uint64_t)(uintptr_t)kernelStackTop);
  wrmsr(SYSENTER_EIP_MSR, (uint64_t)(uintptr_t)sysenterEntry);

  kprintf("Context switching...\n");

  switchContext(schedule(0), 0);
  stopInit("Error: Context switch failed.");
}
