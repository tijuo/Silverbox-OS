#include <elf.h>

#include <kernel/message.h>
#include <kernel/interrupt.h>
#include <kernel/thread.h>
#include <kernel/debug.h>
#include <kernel/mm.h>
#include <kernel/schedule.h>
#include <kernel/cpuid.h>
#include <kernel/paging.h>
#include <kernel/memory.h>
#include <kernel/mm.h>
#include <kernel/pit.h>
#include <kernel/pic.h>
#include <oslib.h>
#include <kernel/error.h>
#include <os/msg/init.h>
#include <os/syscalls.h>

#include <kernel/io.h>
#include <os/variables.h>

#define VGA_RAM             0xA0000
#define VGA_COLOR_TEXT      0xB8000
#define BIOS_ROM            0xC0000
#define EXTENDED_MEMORY     0x100000

#define KERNEL_STACK_SIZE   (8*PAGE_SIZE)
#define DISC_CODE(X) \
    X __attribute__((section(".dtext")))

#define  DISC_DATA(X)  \
    X __attribute__((section(".ddata")))

#define INIT_SERVER_FLAG	"initsrv="

static void switchContext( u32 addrSpace, ExecutionState *state ) __attribute__((section(".dtext")));

static void switchContext( u32 addrSpace, ExecutionState *state )
{
  __asm__ __volatile__(
      "mov %%edx, %%esp\n"
      "mov %%ecx, %%cr3\n"
      "pop %%edi\n"
      "pop %%esi\n"
      "pop %%ebp\n"
      "pop %%ebx\n"
      "pop %%edx\n"
      "pop %%ecx\n"
      "pop %%eax\n"
      "iret\n" :: "ecx"(addrSpace),
      "edx"((dword)state));
}

extern tcb_t *initServerThread;
extern paddr_t *freePageStack;
extern paddr_t *freePageStackTop;

static tcb_t *DISC_CODE(loadElfExe( addr_t, paddr_t, addr_t ));
static bool DISC_CODE(isValidElfExe( addr_t img ));
static void DISC_CODE(initInterrupts( void ));
static void DISC_CODE(initScheduler(void));
static void DISC_CODE(initTimer( void ));
static int DISC_CODE(initMemory( multiboot_info_t *info ));
//int DISC_CODE(add_gdt_entry(int sel, dword base, dword limit, int flags));
static void DISC_CODE(setupGDT(void));
static void DISC_CODE(stopInit(const char *));
static void DISC_CODE(bootstrapInitServer(multiboot_info_t *info));
void DISC_CODE(init(multiboot_info_t *));
static void DISC_CODE(initPIC( void ));
static int DISC_CODE(memcmp(const char *s1, const char *s2, register size_t n));
static int DISC_CODE(strncmp(const char *, const char *, size_t num));
//static size_t DISC_CODE(strlen(const char *s));
static char *DISC_CODE(strstr(char *, const char *));
static char *DISC_CODE(strchr(char *, int));

static void DISC_CODE(showCPU_Features(void));
static void DISC_CODE(showMBInfoFlags(multiboot_info_t *));

static void DISC_CODE(initStructures(multiboot_info_t *));
/*
static unsigned DISC_CODE(bcd2bin(unsigned num));
static unsigned long long DISC_CODE(mktime(unsigned int year, unsigned int month, unsigned int day, unsigned int hour,
                          unsigned int minute, unsigned int second));
 */
static addr_t DISC_DATA(initServerImg);
static bool DISC_CODE(isReservedPage(paddr_t addr, multiboot_info_t * info,
                                     int isLargePage));
//static void DISC_CODE( readPhysMem(addr_t address, addr_t buffer, size_t len) );
static void DISC_CODE(initPageAllocator(multiboot_info_t * info));
DISC_DATA(static paddr_t lastKernelFreePage);
extern void invalidatePage( addr_t );

DISC_DATA(static void (*procExHandlers[20])(void)) = {
    intHandler0, intHandler1, intHandler2, intHandler3, intHandler4, intHandler5,
    intHandler6, intHandler7, intHandler8, intHandler9, intHandler10, intHandler11,
    intHandler12, intHandler13, intHandler14, invalidIntHandler, intHandler16, intHandler17,
    intHandler18, intHandler19
};

DISC_DATA(static void (*procIrqHandlers[16])(void)) = {
    (void (*)(void))timerInt, irq1Handler, irq2Handler, irq3Handler, irq4Handler, irq5Handler,
    irq6Handler, irq7Handler, irq8Handler, irq9Handler, irq10Handler, irq11Handler,
    irq12Handler, irq13Handler, irq14Handler, irq15Handler
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

bool isReservedPage(paddr_t addr, multiboot_info_t * info, int isLargePage)
{
  unsigned int kernelStart = (unsigned int)&kPhysStart;
  unsigned int kernelLength = (unsigned int)&kSize;
  paddr_t addrEnd;

  if(isLargePage)
  {
    addr = addr & ~(PAGE_TABLE_SIZE-1);
    addrEnd = addr + PAGE_TABLE_SIZE;
  }
  else
  {
    addr = addr & ~(PAGE_SIZE-1);
    addrEnd = addr + PAGE_SIZE;
  }

  if(addr < (paddr_t)EXTENDED_MEMORY)
    return true;
  else if((addr >= kernelStart && addr < kernelStart + kernelLength)
      || (addrEnd >= kernelStart && addrEnd < kernelStart+kernelLength))
    return true;
  else
  {
    int inSomeRegion = 0;
    const memory_map_t *mmap;

    for(unsigned long offset=0; offset < info->mmap_length; offset += mmap->size+sizeof(mmap->size))
    {
      mmap = (const memory_map_t *)(info->mmap_addr + offset);

      u64 mmapLen = mmap->length_high;
      mmapLen = mmapLen << 32;
      mmapLen |= mmap->length_low;

      u64 mmap_base = mmap->base_addr_high;
      mmap_base = mmap_base << 32;
      mmap_base |= mmap->base_addr_low;

      if(((u64)addr >= mmap_base && (u64)addr <= mmap_base + mmapLen)
          || ((u64)addrEnd >= mmap_base && (u64)addrEnd <= mmap_base + mmapLen))
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

    module_t *module = (module_t *)info->mods_addr;

    for(unsigned int i=0; i < info->mods_count; i++, module++)
    {
      if(addr > module->mod_start && addr >= module->mod_end
          && addrEnd > module->mod_start && addrEnd > module->mod_end)
        continue;
      else if(addr < module->mod_start && addr < module->mod_end &&
          addrEnd <= module->mod_start && addrEnd < module->mod_end)
        continue;
      else
        return true;
    }

    return false;
  }
}

void initPageAllocator(multiboot_info_t *info)
{
  const memory_map_t *mmap;

  /* 1. Look for a region of page-aligned contiguous memory
        in the size of a large page (2 MiB/4 MiB) that will
        hold the page stack entries.

        If no such region exists then use small sized pages.
        An extra page will be needed as the page table.

     2. For each 2 MiB/4 MiB region of memory, insert the available
        page frame numbers into the stack. Page frames corresponding
        to reserved memory, kernel memory (incl. page stack), MMIO,
        and boot modules are not added.

     3. Repeat until all available page frames are added to the page
        frame stack.
   */

  /*      kprintf("Multiboot MMAP base: 0x%x Multiboot MMAP length: 0x%x\n", info->mmap_addr, info->mmap_length);

      for(unsigned long offset=0; offset < info->mmap_length; offset += mmap->size+sizeof(mmap->size))
      {
        mmap = (const memory_map_t *)(info->mmap_addr + offset);

        u64 mmapLen = mmap->length_high;
        mmapLen = mmapLen << 32;
        mmapLen |= mmap->length_low;

       u64 mmap_base = mmap->base_addr_high;
       mmap_base = mmap_base << 32;
       mmap_base |= mmap->base_addr_low;

       kprintf("Base Addr: 0x");

       if(mmap->base_addr_high)
         kprintf("%x", mmap->base_addr_high);

       kprintf("%x Length: 0x", mmap->base_addr_low);

       if(mmap->length_high)
         kprintf("%x", mmap->length_high);

       kprintf("%x Type: 0x%x Size: 0x%x\n", mmap->length_low, mmap->type, mmap->size);
     }
   */
  size_t pageStackSize = sizeof(paddr_t)*(info->mem_upper) / (16 * 4);
  size_t stackSizeLeft;
  size_t tcbSizeLeft;

  if(pageStackSize % PAGE_SIZE)
    pageStackSize += PAGE_SIZE - (pageStackSize & (PAGE_SIZE-1));

  stackSizeLeft = pageStackSize;
  tcbSizeLeft = (size_t)&kTcbTableSize;

  if(tcbSizeLeft % largePageSize)
    tcbSizeLeft += largePageSize - (tcbSizeLeft & (largePageSize-1));

  kprintf("Page Stack size: %d\n", pageStackSize);
  kprintf("TCB Table size: %d\n", tcbSizeLeft);

  if(info->flags & MBI_FLAGS_MMAP)
  {
    paddr_t largePages[32];
    paddr_t pageTables[32];
    paddr_t tcbPages[2];

    size_t pageTableCount=0; // Number of addresses corresponding to page tables,
    size_t largePageCount=0; // large pages,
    size_t tcbPageCount=0;   // and TCBs in each of the previous arrays

    unsigned int isPageStackMapped=0;
    unsigned int smallPageMapCount=0;
    addr_t mapPtr = (addr_t)freePageStack;

    for(int pageSearchPhase=0; pageSearchPhase <= 2; pageSearchPhase++)
    {
      if(isPageStackMapped)
        break;

      for(unsigned long offset=0; offset < info->mmap_length; offset += mmap->size+sizeof(mmap->size))
      {
        if(isPageStackMapped)
          break;

        mmap = (const memory_map_t *)(info->mmap_addr + offset);

        u64 mmapLen = mmap->length_high;
        mmapLen = mmapLen << 32;
        mmapLen |= mmap->length_low;

        if(mmap->type == MBI_TYPE_AVAIL /*&& mmapLen >= pageStackSize*/)
        {
          paddr_t baseAddr = mmap->base_addr_high;
          unsigned long extraSpace;
          baseAddr = baseAddr << 32;
          baseAddr |= mmap->base_addr_low;

          if(baseAddr >= 0x100000000ull) // Ignore physical addresses greater than 4 GiB
            continue;

          if(pageSearchPhase == 0)      // Look for large page-sized memory for tcb table
          {
            unsigned int tcbPagesSearched = 0;
            extraSpace = mmap->base_addr_low & (largePageSize-1);

            if(extraSpace != 0)
              baseAddr += largePageSize - extraSpace;

            while(tcbSizeLeft >= largePageSize
                && mmapLen >= extraSpace + (tcbPagesSearched+1)*largePageSize)
            {
              if(!isReservedPage(baseAddr + largePageSize * tcbPagesSearched, info, 1))
              {
                tcbPages[tcbPageCount] = baseAddr + largePageSize * tcbPagesSearched;
                tcbSizeLeft -= largePageSize;

                kMapPage(((addr_t)tcbTable + largePageSize*tcbPageCount), tcbPages[tcbPageCount],
                         PAGING_RW | PAGING_SUPERVISOR | PAGING_4MB_PAGE | PAGING_GLOBAL);
                tcbPageCount++;
              }

              tcbPagesSearched++;
            }
          }
          else if(pageSearchPhase == 1) // Looking for large page-sized memory
          {
            unsigned int largePagesSearched = 0; // Number of 4 MiB pages found in this memory region
            extraSpace = mmap->base_addr_low & (largePageSize-1);

            if(extraSpace != 0)
              baseAddr += largePageSize - extraSpace; // Make sure the address we'll use is aligned to a 4 MiB boundary.

            // Find and map all large-sized pages in the memory region that we'll need

            while(stackSizeLeft >= largePageSize
                && mmapLen >= extraSpace + (largePagesSearched+1)*largePageSize)
            {
              if(!isReservedPage(baseAddr + largePageSize * largePagesSearched, info, 1))
              {
                largePages[largePageCount] = baseAddr + largePageSize * largePagesSearched;
                stackSizeLeft -= largePageSize;

                //kprintf("Mapping 4 MB page phys 0x%llx to virt 0x%x\n", largePages[largePageCount], mapPtr);

                kMapPage(mapPtr, largePages[largePageCount],
                         PAGING_RW | PAGING_SUPERVISOR | PAGING_4MB_PAGE);
                largePageCount++;
                mapPtr += largePageSize;
              }

              largePagesSearched++;
            }
          }
          else if(pageSearchPhase == 2)// Looking for small page-sized memory
          {
            unsigned int smallPagesSearched = 0; // Number of 4 KiB pages found in this memory region
            unsigned int pageTablesNeeded = 0;
            extraSpace = mmap->base_addr_low & (PAGE_SIZE-1);

            if(baseAddr % PAGE_SIZE)
              baseAddr += PAGE_SIZE - (baseAddr & (PAGE_SIZE-1));

            // Map the remaining regions with 4 KiB pages

            while(stackSizeLeft > 0
                && mmapLen >= extraSpace + (pageTablesNeeded+smallPagesSearched+1)*PAGE_SIZE)
            {
              if(isReservedPage(baseAddr + PAGE_SIZE * (smallPagesSearched+pageTablesNeeded), info, 0))
              {
                smallPagesSearched++;
                continue;
              }

              // Locate the memory to be used for the page table

              if(smallPageMapCount % 1024 == 0)
              {
                if(mmapLen < extraSpace + (pageTablesNeeded+smallPagesSearched+2)*PAGE_SIZE)
                  break;

                pde_t *pde = ADDR_TO_PDE(mapPtr);

                if(!pde->present)
                {
                  pageTables[pageTableCount] = baseAddr + PAGE_SIZE * (smallPagesSearched+pageTablesNeeded);
                  clearPhysPage(pageTables[pageTableCount]);

                  //kprintf("Mapping page table phys 0x%llx to virt 0x%x\n", pageTables[pageTableCount], mapPtr);

                  *(dword *)pde = pageTables[pageTableCount] | PAGING_RW | PAGING_SUPERVISOR | PAGING_PRES;
                  pageTableCount++;
                  pageTablesNeeded++;
                }
              }

              //kprintf("Mapping page phys 0x%llx to virt 0x%x\n", baseAddr + PAGE_SIZE * (smallPagesSearched+pageTablesNeeded), mapPtr);

              kMapPage(mapPtr, baseAddr + PAGE_SIZE * (smallPagesSearched+pageTablesNeeded),
                       PAGING_RW | PAGING_SUPERVISOR);
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

    unsigned int pagesAdded=0;

    for(unsigned long offset=0; offset < info->mmap_length; offset += mmap->size+sizeof(mmap->size))
    {
      mmap = (const memory_map_t *)(info->mmap_addr + offset);

      if(mmap->type == MBI_TYPE_AVAIL)
      {
        u64 mmapLen = mmap->length_high;
        mmapLen = mmapLen << 32;
        mmapLen |= mmap->length_low;

        u64 mmapAddr = mmap->base_addr_high;
        mmapAddr = mmapAddr << 32;
        mmapAddr |= mmap->base_addr_low;

        if(mmapAddr & (PAGE_SIZE-1))
        {
          u64 extraSpace = PAGE_SIZE - (mmapAddr & (PAGE_SIZE-1));

          mmapAddr += extraSpace;
          mmapLen -= extraSpace;
        }

        // Check each address in this region to determine if it should be added to the free page list

        for(paddr_t paddr=mmapAddr, end=mmapAddr+mmapLen; paddr < end && pagesAdded < pageStackSize / sizeof(paddr_t); )
        {
          int found = 0;

          // Is this address within the tcb table?

          for(size_t i=0; i < tcbPageCount; i++)
          {
            if(paddr >= tcbPages[i] && paddr < tcbPages[i] + largePageSize)
            {
              found = 1;
              paddr = tcbPages[i] + largePageSize;
              break;
            }
          }

          if(found)
            continue;

          // Is this address within a large page?

          for(size_t i=0; i < largePageCount; i++)
          {
            if(paddr >= largePages[i] && paddr < largePages[i] + largePageSize)
            {
              found = 1;
              paddr = largePages[i] + largePageSize;
              break;
            }
          }

          if(found)
            continue;

          // Is this page used as a page table?

          for(size_t i=0; i < pageTableCount; i++)
          {
            if(paddr >= pageTables[i] && paddr < pageTables[i] + PAGE_SIZE)
            {
              found = 1;
              paddr += PAGE_SIZE;
              break;
            }
          }

          if(found)
            continue;

          // Is this page reserved?

          if(isReservedPage(paddr, info, 0))
          {
            paddr += PAGE_SIZE;
            continue;
          }

          // Read each PTE that maps to the page stack and compare with pte.base, if a match, mark it as found

          for(addr_t addr=((addr_t)freePageStack); addr < (((addr_t)freePageStack) + (int)pageStackSize); addr += PAGE_SIZE)
          {
            pte_t *pte = ADDR_TO_PTE(addr);

            if((paddr_t)PFRAME_TO_ADDR(pte->base) == paddr)
            {
              found = 1;
              paddr += PAGE_SIZE;
              break;
            }
          }

          if(found)
            continue;
          else
          {
            *freePageStackTop = paddr;
            freePageStackTop++;
            lastKernelFreePage = paddr;
            paddr += PAGE_SIZE;
            pagesAdded++;
          }
        }
      }
    }
    kprintf("Page allocator initialized. Added %d pages to free stack.\n", pagesAdded);
  }
  else
  {
    kprintf("Multiboot memory map is missing.\n");
    assert(false);
  }
}

int memcmp(const char *s1, const char *s2, register size_t n)
{
  for( ; n && *s1 == *s2; n--, s1++, s2++);

  if( !n )
    return 0;
  else
    return (*s1 > *s2 ? 1 : -1);
}

int strncmp( const char * str1, const char * str2, size_t num )
{
  register size_t i;

  if( !str1 && !str2 )
    return 0;
  else if( !str1 )
    return -1;
  else if( !str2 )
    return 1;

  for( i=0; i < num && str1[i] == str2[i] && str1[i]; i++ );

  if( i == num )
    return 0;
  else
    return (str1[i] > str2[i] ? 1 : -1);
}

char *strstr(char * str, const char * substr )
{
  register size_t i;

  if(str && substr)
  {
    for( ; *str; str++ )
    {
      for( i=0; str[i] == substr[i] && substr[i]; i++ );

      if( !substr[i] )
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
char *strchr(char * s, int c)
{
  if(s)
  {
    while(*s)
    {
      if(*s == c)
        return (char *)s;
      else
        s++;
    }

    return c ? NULL : s;
  }

  return NULL;
}

void initPIC( void )
{
  outByte( (word)0x20, 0x11 );
  ioWait();
  outByte( (word)0xA0, 0x11 );
  ioWait();
  outByte( (word)0x21, IRQ0 );
  ioWait();
  outByte( (word)0xA1, IRQ8 );
  ioWait();
  outByte( (word)0x21, 4 );
  ioWait();
  outByte( (word)0xA1, 2 );
  ioWait();
  outByte( (word)0x21, 1 );
  ioWait();
  outByte( (word)0xA1, 1 );
  ioWait();

  outByte( (word)0x21, 0xFF );
  ioWait();
  outByte( (word)0xA1, 0xFF );
  ioWait();


  kprintf("Enabling IRQ 2\n");
  enableIRQ( 2 );
}

/* Various call once functions */

struct gdt_entry {
  word  limit1;
  word  base1;
  byte  base2;
  byte  flags1;
  byte  limit2 : 4;
  byte  flags2 : 4;
  byte  base3;
} __PACKED__;

struct gdt_pointer
{
  word limit;
  dword base;
} __PACKED__;

/*
int add_gdt_entry(unsigned int sel, dword base, dword limit, int flags)
{
  unsigned int rsel = sel;

  kprintf("GDT: Sel- 0x%x Base: 0x%x Limit: 0x%x Flags: 0x%x\n", sel, base, limit, flags);

  struct gdt_entry * const entry = (struct gdt_entry * const)&kernelGDT;

  if((sel < 0))
    return 0;

  entry += (sel / 8);

  entry->limit1 = (word)(limit & 0xFFFFu);
  entry->limit2 = (byte)((limit >> 16) & 0xFu);
  entry->base1 = (word)(base & 0xFFFFu);
  entry->base2 = (byte)((base >> 16) & 0xFFu);
  entry->base3 = (byte)((base >> 24) & 0xFFu);
  entry->flags1 = (byte)(flags & 0xFFu);
  entry->flags2 = (byte)((flags >> 8) & 0x0Fu);

  return rsel;
}
 */

void setupGDT(void)
{
  volatile struct TSS_Struct *tss = (volatile struct TSS_Struct *)EXT_PTR(kernelTSS);
  struct gdt_entry *entry = (struct gdt_entry *)((unsigned int)EXT_PTR(kernelGDT) + TSS);
  struct gdt_pointer gdt_pointer;

  entry->base1 = (dword)tss & 0xFFFFu;
  entry->base2 = (byte)(((dword)tss >> 16) & 0xFFu);
  entry->base3 = (byte)(((dword)tss >> 24) & 0xFFu);
  entry->limit1 = sizeof(struct TSS_Struct) + 2 * PAGE_SIZE; // Size of TSS structure and IO Bitmap
  entry->limit2 = 0;

  tss->ss0 = KDATA;

  __asm__("ltr %%ax" :: "ax"(TSS) );
  __asm__("sgdt %0" : "=m"(gdt_pointer));
  gdt_pointer.limit -= 0x10u;
  __asm__("lgdt %0" :: "m"(gdt_pointer));
}

void stopInit(const char *msg)
{
  disableInt();
  kprintf("Init failed: %s\nSystem halted.", msg);

  while(1)
    __asm__("hlt\n");
}

/* Warning: This is fragile code! Any changes to this function or to the
   functions that are called by this function may break the entire
   kernel. */

int initMemory( multiboot_info_t * info )
{
  unsigned int totalPhysMem = (info->mem_upper + 1024) * 1024;
  pte_t *ptePtr;
  addr_t addr;

  kprintf("Total Memory: 0x%x. %d pages. ", totalPhysMem, totalPhysMem / PAGE_SIZE );

  if( totalPhysMem < (8 << 20) )
    stopInit("Not enough memory!");

  kprintf("Kernel AddrSpace: 0x%x\n", initKrnlPDir);

  // Mark kernel code pages as read-write

  for( addr=(addr_t)EXT_PTR(kCode); addr < (addr_t)EXT_PTR(kData); addr += PAGE_SIZE )
  {
    ptePtr = ADDR_TO_PTE( addr );

    ptePtr->rwPriv = 0;
    invalidatePage( addr );
  }

  setupGDT();

  return 0;
}

void initTimer( void )
{
  addIDTEntry( timerHandler, IRQ0, INT_GATE | KCODE );

  outByte( (word)TIMER_CTRL, (byte)(C_SELECT0 | C_MODE3 | BIN_COUNTER | RWL_FORMAT3) );
  outByte( (word)TIMER0, (byte)(( TIMER_FREQ / TIMER_QUANTA_HZ ) & 0xFF) );
  outByte( (word)TIMER0, (byte)(( TIMER_FREQ / TIMER_QUANTA_HZ ) >> 8) );

  kprintf("Enabling IRQ 0\n");
  enableIRQ( 0 );
}

void initScheduler( void)
{
  initTimer();
}

void initInterrupts( void )
{
  for(unsigned int i=0; i <= 0xFF; i++)
  {
    void *handler;

    if(i < 20)
      handler = (void *)procExHandlers[i];
    else if(i >= IRQ0 && i <= IRQ15)
      handler = (void *)procIrqHandlers[i - IRQ0];
    else if(i == SYSCALL_INT)
      handler = syscallHandler;
    else
      handler = invalidIntHandler;

    addIDTEntry(handler, i, INT_GATE | KCODE | (i == SYSCALL_INT ? I_DPL3 : I_DPL0));
  }

  initPIC();
  loadIDT();
}

bool isValidElfExe( addr_t img )
{
  elf_header_t *image = ( elf_header_t * ) img;

  if( img == NULL )
    return false;

  if ( memcmp( "\x7f""ELF", image->identifier, 4 ) != 0 )
    return false;

  else
  {
    if ( image->identifier[ EI_VERSION ] != EV_CURRENT )
      return false;
    if ( image->type != ET_EXEC )
      return false;
    if ( image->machine != EM_386 )
      return false;
    if ( image->version != EV_CURRENT )
      return false;
    if ( image->identifier[ EI_CLASS ] != ELFCLASS32 )
      return false;
  }

  return true;

}

tcb_t *loadElfExe( addr_t img, paddr_t addrSpace, addr_t uStack )
{
  elf_header_t image;
  elf_sheader_t sheader;
  tcb_t *thread;
  paddr_t page;
  pde_t pde;
  pte_t pte;
  size_t i;
  size_t offset;

  peek( img, &image, sizeof image );
  pte.present = 0;

#if DEBUG
  int result;
#endif /* DEBUG */

  if( !isValidElfExe( (addr_t)&image ) )
  {
    kprintf("Not a valid ELF executable.\n");
    return NULL;
  }

  thread = createThread( INIT_SERVER_TID, (addr_t)image.entry, addrSpace, uStack );

  if( thread == NULL )
  {
    kprintf("loadElfExe(): Couldn't create thread.\n");
    return NULL;
  }

  /* Create the page table before mapping memory */

  for( i=0; i < image.shnum; i++ )
  {
    peek( (img + image.shoff + i * image.shentsize), &sheader, sizeof sheader );

    if( !(sheader.flags & SHF_ALLOC) )
      continue;

    for( offset=0; offset < sheader.size; offset += PAGE_SIZE )
    {
#if DEBUG
      result =
#endif /* DEBUG */

          readPmapEntry(addrSpace, PDE_INDEX(sheader.addr+offset), &pde);
      //readPDE(sheader.addr + offset, &pde, addrSpace);

      assert( result == 0 );

      if( !pde.present )
      {
        page = allocPageFrame();
        clearPhysPage(page);

        pde.base = (u32)ADDR_TO_PFRAME(page);
        pde.rwPriv = 1;
        pde.usPriv = 1;
        pde.present = 1;

#if DEBUG
        result =
#endif /* DEBUG */

            writePmapEntry(addrSpace, PDE_INDEX(sheader.addr+offset), &pde);
        //writePDE((addr_t)sheader.addr + offset, &pde, addrSpace);

        assert( result == 0 );
      }
      else
      {
        readPmapEntry(PFRAME_TO_ADDR(pde.base), PTE_INDEX(sheader.addr+offset), &pte);
        //readPTE(sheader.addr + offset, &pte, addrSpace);
      }

      if( sheader.type == SHT_PROGBITS )
      {
        //kprintf("mapping PROGBITS 0x%x->0x%x\n", (sheader.addr + offset), ((addr_t)img + sheader.offset + offset));

        if( !pte.present )
        {
          pte.base = (u32)ADDR_TO_PFRAME((u32)img + sheader.offset + offset);
          pte.usPriv = 1;
          pte.rwPriv = !!(sheader.flags & SHF_WRITE);
          pte.present = 1;

          if(writePmapEntry(PFRAME_TO_ADDR(pde.base), PTE_INDEX(sheader.addr+offset), &pte) !=0)
            return NULL;

          /*          if( writePTE((addr_t)sheader.addr + offset, &pte, addrSpace) != 0 )
            return NULL; */
          /*
	  mapPage((addr_t)sheader.addr + offset, (addr_t)img + sheader.offset + offset,
            PAGING_USER | (sheader.flags & SHF_WRITE ? PAGING_RW : PAGING_RO), addrSpace); */
        }
      }
      else if( sheader.type == SHT_NOBITS )
      {
        if( !pte.present )
        {
          page = allocPageFrame();
          clearPhysPage( page );
          //kprintf("mapping NOBITS 0x%x->0x%x\n", (sheader.addr + offset), page);

          pte.base = (u32)ADDR_TO_PFRAME(page);
          pte.usPriv = 1;
          pte.rwPriv = !!(sheader.flags & SHF_WRITE);
          pte.present = 1;

          if(writePmapEntry(PFRAME_TO_ADDR(pde.base), PTE_INDEX(sheader.addr+offset), &pte) !=0)
            return NULL;

          /*          if( writePTE((addr_t)sheader.addr + offset, &pte, addrSpace) != 0 )
            return NULL;
           */
          /*
          mapPage((addr_t)sheader.addr + offset, (addr_t)page,
                  PAGING_USER | (sheader.flags & SHF_WRITE ? PAGING_RW : PAGING_RO), addrSpace); */
        }
        else
        {
          memset((void *)(sheader.addr + offset), 0, PAGE_SIZE - (offset % PAGE_SIZE));
        }
      }
    }
  }

  return thread;
}

/**
    Bootstraps the initial server and passes necessary boot data to it.
 */

void bootstrapInitServer(multiboot_info_t *info)
{
  int fail=0;
  addr_t initServerStack = INIT_SERVER_STACK_TOP;
  paddr_t initServerPDir=NULL_PADDR;
  elf_header_t elf_header;
  paddr_t stackPTab = allocPageFrame();
  paddr_t stackPage = allocPageFrame();
  int stackData[2] = { (int)info, (int)lastKernelFreePage };

  kprintf("Bootstrapping initial server...\n");

  peek(initServerImg, &elf_header, sizeof elf_header);

  if(!isValidElfExe( (addr_t)&elf_header ))
    fail = 1;
  else if((initServerPDir = allocPageFrame()) == NULL_PADDR
      || clearPhysPage(initServerPDir) != E_OK)
  {
    fail = 1;
  }
  else
  {
    pde_t pde;
    pte_t pte;

    memset(&pde, 0, sizeof(pde_t));
    memset(&pte, 0, sizeof(pte_t));

    pde.base = (u32)ADDR_TO_PFRAME(stackPTab);
    pde.rwPriv = 1;
    pde.usPriv = 1;
    pde.present = 1;

    pte.base = (u32)ADDR_TO_PFRAME(stackPage);
    pte.rwPriv = 1;
    pte.usPriv = 1;
    pte.present = 1;

    if(stackPTab != NULL_PADDR)
      clearPhysPage(stackPTab);

    if((initServerThread=loadElfExe(initServerImg, initServerPDir,
                                    initServerStack-sizeof(stackData))) == NULL)
    {
      fail = 1;
    }
    else if(stackPTab == NULL_PADDR || stackPage == NULL_PADDR)
      fail = 1;
    else if(writePmapEntry(initServerPDir, PDE_INDEX(initServerStack-PAGE_SIZE),
                           &pde) != E_OK ||
        writePmapEntry(stackPTab, PTE_INDEX(initServerStack-PAGE_SIZE),
                       &pte) != E_OK)
    {
      fail = 1;
    }
    else
    {
      poke(stackPage + PAGE_SIZE - sizeof(stackData), stackData, sizeof(stackData));
      kprintf("Starting initial server... 0x%x\n", initServerThread);

      if(startThread(initServerThread) != E_OK)
        fail = 1;
    }
  }

  if(fail)
  {
    kprintf("Unable to start initial server.\n");

    if(stackPTab != NULL_PADDR)
      freePageFrame(stackPTab);

    if(stackPage != NULL_PADDR)
      freePageFrame(stackPage);

    if(initServerPDir != NULL_PADDR)
      freePageFrame(initServerPDir);
  }
}

#if DEBUG
#if 0
#define MBI_FLAGS_MEM		(1u << 0)  /* 'mem_*' fields are valid */
#define MBI_FLAGS_BOOT_DEV	(1u << 1)  /* 'boot_device' field is valid */
#define MBI_FLAGS_CMDLINE	(1u << 2)  /* 'cmdline' field is valid */
#define MBI_FLAGS_MODS		(1u << 3)  /* 'mods' fields are valid */
#define MBI_FLAGS_SYMTAB		(1u << 4)
#define MBI_FLAGS_SHDR		(1u << 5)  /* 'shdr_*' fields are valid */
#define MBI_FLAGS_MMAP		(1u << 6)  /* 'mmap_*' fields are valid. */
#define MBI_FLAGS_DRIVES		(1u << 7)  /* 'drives_*' fields are valid */
#define MBI_FLAGS_CONFIG		(1u << 8)  /* 'config_table' field is valid */
#define MBI_FLAGS_BOOTLDR	(1u << 9)  /* 'boot_loader_name' field is valid */
#define MBI_FLAGS_APM_TAB	(1u << 10) /* 'apm_table' field is valid */
#define MBI_FLAGS_GFX_TAB	(1u << 11) /* Grahphics table is available */
#endif /* 0 */

void showMBInfoFlags( multiboot_info_t *info )
{
  const char *names[] = { "MEM", "BOOT_DEV", "CMDLINE", "MODS", "SYMTAB", "SHDR",
      "MMAP", "DRIVES", "CONFIG", "BOOTLDR", "APM_TAB", "GFX_TAB" };

  kprintf("Mulitboot Information Flags:\n");

  for(size_t i=0; i < 12; i++)
  {
    if( info->flags & (1u << i) )
      kprintf("%s\n", names[i]);
  }
}

void showCPU_Features(void)
{
  unsigned int features;

  __asm__ __volatile__("cpuid\n" : "=edx"(features) : "eax"(0x01) : "%ebx", "%ecx");

  if( cpuFeatures.ia64 )
    kprintf("IA-64\n");
  if( cpuFeatures.mmx )
    kprintf("MMX\n");
  if( cpuFeatures.pse36 )
    kprintf("PSE36\n");
  if( cpuFeatures.pge )
    kprintf("PGE\n");
  if( cpuFeatures.sse )
    kprintf("SSE\n");
  if( cpuFeatures.sse2 )
    kprintf("SSE2\n");
  if( cpuFeatures.sep )
    kprintf("sysenter/sysexit\n");
  if( cpuFeatures.msr )
    kprintf("MSR\n");
  if( cpuFeatures.pse )
    kprintf("PSE\n");
}
#endif /* DEBUG */

void initStructures(multiboot_info_t *info)
{
  kprintf("Initializing free page allocator.\n");
  initPageAllocator(info);

  // Map every page table in the kernel's region of memory. This will be copied
  // whenever a new address space is created, so that the kernel will be present
  // in each.

  for( addr_t addr=(addr_t)KERNEL_TCB_START; addr < PAGETAB; addr += PAGE_TABLE_SIZE )
  {
    pde_t *pdePtr = ADDR_TO_PDE(addr);

    if(!pdePtr->present)
    {
      paddr_t phys = allocPageFrame();

      clearPhysPage(phys);
      pdePtr->base = (u32)ADDR_TO_PFRAME(phys);
      pdePtr->rwPriv = 1;
      pdePtr->usPriv = 0;
      pdePtr->present = 1;
    }
  }

  memset(tcbTable, 0, (int)&kTcbTableSize);
}

/**
    Bootstraps the kernel.

    @param info The multiboot structure passed by the bootloader.
 */

void init( multiboot_info_t *info )
{
  //  memory_map_t *mmap;
  module_t *module;
  unsigned int i=0;
  bool initServerFound=false;
  char *initServerStrPtr = NULL;
  char *initServerStrEnd=NULL;
  pte_t *pte;

#ifdef DEBUG
  init_serial();
  initVideo();
  setVideoLowMem( true );
  clearScreen();
  showMBInfoFlags(info);
  showCPU_Features();
#endif

  initServerStrPtr = strstr( (char *)info->cmdline, INIT_SERVER_FLAG );

  /* Locate the initial server string (if it exists) */

  if( initServerStrPtr )
  {
    initServerStrPtr += (sizeof( INIT_SERVER_FLAG ) - 1);
    initServerStrEnd = strchr(initServerStrPtr, ' ');

    if( !initServerStrEnd )
      initServerStrEnd = strchr(initServerStrPtr, '\0');
  }
  else
  {
    kprintf("Initial server not specified.\n");
    stopInit("No boot modules found.\nNo initial server to start.");
  }

  if( info->flags & 1 )
    kprintf("Lower Memory: %d B Upper Memory: %d B\n", info->mem_lower << 10, info->mem_upper << 10);

  kprintf("Boot info struct: 0x%x\nModules located at 0x%x. %d modules\n",
          info, info->mods_addr, info->mods_count);

  module = (module_t *)info->mods_addr;

  /* Copy the boot modules and locate the initial server. */

  for(i=info->mods_count; i; i--, module++)
  {
    if( strncmp((char *)module->string, initServerStrPtr,
                initServerStrEnd-initServerStrPtr) == 0 )
    {
      initServerImg = (addr_t)module->mod_start;
      initServerFound = true;
      break;
    }
  }

  if( !initServerFound )
    stopInit("Can't find initial server.");

  kprintf("%d run queues.\n", NUM_RUN_QUEUES);

  /* Initialize memory */

  if( initMemory( info ) < 0 )
    stopInit("Not enough memory! At least 8 MiB is needed.");

  kprintf("Initializing interrupt handling.\n");
  initInterrupts();

  kprintf("Initializing data structures.\n");
  initStructures(info);

  //  enable_apic();
  //  init_apic_timer();
  kprintf("Initializing scheduler.\n");
  initScheduler();

  bootstrapInitServer(info);

  kprintf("\n0x%x bytes of discardable code.", (addr_t)EXT_PTR(kdData) - (addr_t)EXT_PTR(kdCode));
  kprintf(" 0x%x bytes of discardable data.\n", (addr_t)EXT_PTR(kBss) - (addr_t)EXT_PTR(kdData));
  kprintf("Discarding %d bytes in total\n", (addr_t)EXT_PTR(kBss) - (addr_t)EXT_PTR(kdCode));

  /* Release the pages for the code and data that will never be used again. */

  for( addr_t addr=(addr_t)EXT_PTR(kdCode); addr < (addr_t)EXT_PTR(kBss); addr += PAGE_SIZE )
    freePageFrame((paddr_t)(PFRAME_TO_ADDR(ADDR_TO_PTE(addr)->base)));

  // XXX: Release any pages tables used for discardable sections

  freePageFrame((paddr_t)k1To1PageTable);

  freePageFrame((paddr_t)(bootStackTop-PAGE_SIZE));

  *(dword *)EXT_PTR(tssEsp0) = (dword)kernelStackTop;

  schedule();

  // Unmap any unneeded mapped pages in the kernel's address space

  for(addr_t addr=(addr_t)0x0000; addr < (addr_t)largePageSize; addr += PAGE_SIZE)
  {
    if(addr == kernelStackTop-KERNEL_STACK_SIZE)
      addr = (addr_t)VGA_RAM;

#if DEBUG

    if(addr == VGA_COLOR_TEXT)
    {
      addr = (addr_t)BIOS_ROM;
      continue;
    }
#endif  /* DEBUG */

    pte = ADDR_TO_PTE(addr);

    if(pte->present)
    {
      pte->present = 0;
      invalidatePage(addr);
    }
  }

  kprintf("Context switching...\n");

  switchContext( initServerThread->rootPageMap, &initServerThread->execState );
  stopInit("Error: Context switch failed.");
}
