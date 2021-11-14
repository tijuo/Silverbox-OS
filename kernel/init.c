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

#define DISC_CODE SECTION(".dtext")

#define DISC_DATA SECTION(".ddata")

#define INIT_SERVER_FLAG	"initsrv="

extern pte_t kMapAreaPTab[PTE_ENTRY_COUNT];

DISC_DATA multiboot_info_t *multibootInfo;
DISC_DATA ALIGNED(PAGE_SIZE) uint8_t kBootStack[4 * PAGE_SIZE];
DISC_DATA void *kBootStackTop = kBootStack + sizeof kBootStack;
DISC_DATA ALIGNED(PAGE_SIZE) pmap_entry_t kPageDir[PMAP_ENTRY_COUNT]; // The initial page directory used by the kernel on bootstrap

extern gdt_entry_t kernelGDT[8];
extern idt_entry_t kernelIDT[NUM_EXCEPTIONS + NUM_IRQS];

extern tcb_t *initServerThread;
extern uint8_t *kernelStackTop;

//DISC_CODE static bool isValidAcpiHeader(paddr_t physAddress));
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
//DISC_CODE static int memcmp(const char *s1, const char *s2, register size_t n));
DISC_CODE static int strncmp(const char*, const char*, size_t num);
//DISC_CODE static size_t strlen(const char *s));
DISC_CODE static char* strstr(char*, const char*);
DISC_CODE static char* strchr(char*, int);
DISC_CODE void addIDTEntry(void (*f)(void), unsigned int entryNum,
                           unsigned int dpl);
DISC_CODE void loadIDT(void);
DISC_CODE addr_t allocPageFrame(void);

/*
 static unsigned DISC_CODE bcd2bin(unsigned num));
 static unsigned long long DISC_CODE mktime(unsigned int year, unsigned int month, unsigned int day, unsigned int hour,
 unsigned int minute, unsigned int second));
 */
DISC_DATA static addr_t initServerImg;
DISC_CODE static bool isReservedPage(paddr_t addr, multiboot_info_t *info,
                                     int isLargePage);

//paddr_t DISC_CODE findRSDP(struct RSDPointer *header));
//paddr_t DISC_CODE findMP_Header(struct MP_FloatingHeader *header));
//static void DISC_CODE  readPhysMem(addr_t address, addr_t buffer, size_t len) );

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

addr_t allocPageFrame(void) {
  addr_t frame = firstFreePage;

  while(frame >= EXTENDED_MEMORY && isReservedPage(frame, multibootInfo, 0)) {
    frame += PAGE_SIZE;
  }

  if(frame < EXTENDED_MEMORY)
    return INVALID_PFRAME ;

  firstFreePage = frame + PAGE_SIZE;

  return frame;
}

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
      writePDE(PDE_INDEX(initServerStack-PAGE_SIZE), pde, initServerPDir))
     || IS_ERROR(writePTE(PTE_INDEX(initServerStack-PAGE_SIZE), pte, stackPTab)))
  {
    kprintf("Unable to write page map entries for init server stack.\n");
    goto failedBootstrap;
  }

  if((initServerThread = loadElfExe(
      initServerImg, initServerPDir,
      (void*)(initServerStack - sizeof(stackData))))
     == NULL) {
    kprintf("Unable to load ELF executable.\n");
    goto failedBootstrap;
  }

  poke(stackPage + PAGE_SIZE - sizeof(stackData), &stackData,
       sizeof(stackData));
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

  /* Map lower 2 GiB of physical memory to kernel space.
   * The kernel has to be careful not to write to read-only or non-existent areas.
   */

  paddr_t physAddr = 0;

  for(size_t pdeIndex = PDE_INDEX(KERNEL_VSTART);
      pdeIndex < LARGE_PDE_ENTRY_COUNT; pdeIndex++, physAddr += LARGE_PAGE_SIZE)
  {
    large_pde_t *largePde = &kPageDir[pdeIndex].largePde;

    setLargePdeBase(largePde, physAddr);

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

  // Paging hasn't been initialized yet. (Debugger may not work property until initPaging() is called.)

  initPaging();

  info = (multiboot_info_t*)KPHYS_TO_VIRT(info);
  multibootInfo = info;

  firstFreePage = KVIRT_TO_PHYS((addr_t )&kTcbEnd);

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
    kprintf("Lower Memory: %d bytes Upper Memory: %d bytes\n",
            info->mem_lower << 10, info->mem_upper << 10);

  kprintf("Boot info struct: %#p\nModules located at %#p. %d modules\n", info,
          info->mods_addr, info->mods_count);

  module = (module_t*)KPHYS_TO_VIRT(info->mods_addr);

  /* Locate the initial server module. */

  for(size_t i = info->mods_count; i; i--, module++) {
    if(strncmp((char*)KPHYS_TO_VIRT(module->string), initServerStrPtr,
               initServerStrEnd - initServerStrPtr)
       == 0) {
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
  wrmsr(SYSENTER_EIP_MSR, (uint64_t)(uintptr_t)sysenter);

  kprintf("Context switching...\n");
  while(1)
    ;

  switchContext(initServerThread, 0);
  stopInit("Error: Context switch failed.");
}
