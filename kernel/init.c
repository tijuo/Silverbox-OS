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
#include <kernel/dlmalloc.h>
#include <kernel/error.h>
#include <os/msg/init.h>

#include <kernel/io.h>
#include <os/variables.h>

#define DISC_CODE(X) \
  X __attribute__((section(".dtext")))

#define  DISC_DATA(X)  \
  X __attribute__((section(".ddata")))

#define INIT_SERVER_FLAG	"initsrv="

static inline void switchContext( u32 addrSpace, ExecutionState state ) __attribute__((section(".dtext")));

static inline void switchContext( u32 addrSpace, ExecutionState state )
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
    "edx"((dword)&state));
}

extern tcb_t *initServerThread;
extern paddr_t *freePageStack, *freePageStackTop;

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
static int DISC_CODE(strncmp(const char * restrict, const char * restrict, size_t num));
//static size_t DISC_CODE(strlen(const char *s));
static char *DISC_CODE(strstr(const char * restrict, const char * restrict));
static char *DISC_CODE(strchr(const char * restrict, int));
static void DISC_CODE(showCPU_Features(void));
static void DISC_CODE(showMBInfoFlags(multiboot_info_t *));
static void DISC_CODE(initStructures(multiboot_info_t *));
/*
static unsigned DISC_CODE(bcd2bin(unsigned num));
static unsigned long long DISC_CODE(mktime(unsigned int year, unsigned int month, unsigned int day, unsigned int hour,
                          unsigned int minute, unsigned int second));
*/
static addr_t DISC_DATA(initServerImg);
static bool DISC_CODE(isReservedPage(paddr_t addr, multiboot_info_t * restrict info,
                                     int isLargePage));
//static void DISC_CODE( readPhysMem(addr_t address, addr_t buffer, size_t len) );
static void DISC_CODE(initPageAllocator(multiboot_info_t * restrict info));
static paddr_t DISC_DATA(lastKernelFreePage);
extern void invalidatePage( addr_t );

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

bool isReservedPage(paddr_t addr, multiboot_info_t * restrict info, int isLargePage)
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

  if(addr < (paddr_t)0x100000)
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
  size_t stackSizeLeft, tcbSizeLeft;

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
    paddr_t largePages[32], pageTables[32], tcbPages[2];
    unsigned int pageTableCount=0, largePageCount=0, tcbPageCount=0; // Number of addresses corresponding to page tables and large pages in each of the previous arrays

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

            if((paddr_t)(pte->base << 12) == paddr)
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

int strncmp( const char * restrict str1, const char * restrict str2, size_t num )
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

char *strstr( const char * restrict str, const char * restrict substr )
{
  register size_t i;
  register size_t off;

  if( !str )
    return NULL;
  else if( !substr || !substr[0] )
    return (char *)str;

  for( off=0; str[off]; off++ )
  {
    for( i=0; str[off+i] == substr[i] && substr[i]; i++ );

    if( !substr[i] )
      return (char *)str;
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
char *strchr(const char * restrict s, int c)
{
  register size_t i;

  if( !s )
    return NULL;

  for( i=0; s[i] != c && s[i]; i++ );

  if( s[i] == c )
    return (char *)s;
  else
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

int initMemory( multiboot_info_t * restrict info )
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
  outByte( (word)TIMER0, (byte)(( TIMER_FREQ / HZ ) & 0xFF) );
  outByte( (word)TIMER0, (byte)(( TIMER_FREQ / HZ ) >> 8) );

  kprintf("Enabling IRQ 0\n");
  enableIRQ( 0 );
}

void initScheduler( void)
{
  initTimer();
}

void initInterrupts( void )
{
  unsigned int i;

  addIDTEntry( ( void * ) intHandler0, 0, INT_GATE | KCODE );
  addIDTEntry( ( void * ) intHandler1, 1, INT_GATE | KCODE );
  addIDTEntry( ( void * ) intHandler2, 2, INT_GATE | KCODE );
  addIDTEntry( ( void * ) intHandler3, 3, INT_GATE | KCODE );
  addIDTEntry( ( void * ) intHandler4, 4, INT_GATE | KCODE );
  addIDTEntry( ( void * ) intHandler5, 5, INT_GATE | KCODE );
  addIDTEntry( ( void * ) intHandler6, 6, INT_GATE | KCODE );
  addIDTEntry( ( void * ) intHandler7, 7, INT_GATE | KCODE );
  addIDTEntry( ( void * ) intHandler8, 8, INT_GATE | KCODE );
  addIDTEntry( ( void * ) intHandler9, 9, INT_GATE | KCODE );
  addIDTEntry( ( void * ) intHandler10, 10, INT_GATE | KCODE );
  addIDTEntry( ( void * ) intHandler11, 11, INT_GATE | KCODE );
  addIDTEntry( ( void * ) intHandler12, 12, INT_GATE | KCODE );
  addIDTEntry( ( void * ) intHandler13, 13, INT_GATE | KCODE );
  addIDTEntry( ( void * ) intHandler14, 14, INT_GATE | KCODE );
  addIDTEntry( ( void * ) invalidIntHandler, 15, INT_GATE | KCODE );
  addIDTEntry( ( void * ) intHandler16, 16, INT_GATE | KCODE );
  addIDTEntry( ( void * ) intHandler17, 17, INT_GATE | KCODE );
  addIDTEntry( ( void * ) intHandler18, 18, INT_GATE | KCODE );
  addIDTEntry( ( void * ) intHandler19, 19, INT_GATE | KCODE );

  addIDTEntry( ( void * ) irq1Handler, 0x21, INT_GATE | KCODE ) ;
  addIDTEntry( ( void * ) irq2Handler, 0x22, INT_GATE | KCODE ) ;
  addIDTEntry( ( void * ) irq3Handler, 0x23, INT_GATE | KCODE ) ;
  addIDTEntry( ( void * ) irq4Handler, 0x24, INT_GATE | KCODE ) ;
  addIDTEntry( ( void * ) irq5Handler, 0x25, INT_GATE | KCODE ) ;
  addIDTEntry( ( void * ) irq6Handler, 0x26, INT_GATE | KCODE ) ;
  addIDTEntry( ( void * ) irq7Handler, 0x27, INT_GATE | KCODE ) ;
  addIDTEntry( ( void * ) irq8Handler, 0x28, INT_GATE | KCODE ) ;
  addIDTEntry( ( void * ) irq9Handler, 0x29, INT_GATE | KCODE ) ;
  addIDTEntry( ( void * ) irq10Handler, 0x2A, INT_GATE | KCODE ) ;
  addIDTEntry( ( void * ) irq11Handler, 0x2B, INT_GATE | KCODE ) ;
  addIDTEntry( ( void * ) irq12Handler, 0x2C, INT_GATE | KCODE ) ;
  addIDTEntry( ( void * ) irq13Handler, 0x2D, INT_GATE | KCODE ) ;
  addIDTEntry( ( void * ) irq14Handler, 0x2E, INT_GATE | KCODE ) ;
  addIDTEntry( ( void * ) irq15Handler, 0x2F, INT_GATE | KCODE ) ;

  for ( i = 20; i < 32; i++ )
    addIDTEntry( ( void * ) invalidIntHandler, i, INT_GATE | KCODE );

  for( i = 0x30; i <= 0xFF; i++ )
    addIDTEntry( ( void *) invalidIntHandler, i, INT_GATE | KCODE );

  addIDTEntry( ( void * ) syscallHandler, 0x40, INT_GATE | KCODE | I_DPL3 );

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
  size_t i, offset;

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

        *(u32 *)&pde = (u32)page | PAGING_RW | PAGING_USER | PAGING_PRES;

        #if DEBUG
          result =
        #endif /* DEBUG */

        writePmapEntry(addrSpace, PDE_INDEX(sheader.addr+offset), &pde);
        //writePDE((addr_t)sheader.addr + offset, &pde, addrSpace);

        assert( result == 0 );
      }
      else
      {
        readPmapEntry(pde.base << 12, PTE_INDEX(sheader.addr+offset), &pte);
        //readPTE(sheader.addr + offset, &pte, addrSpace);
      }

      if( sheader.type == SHT_PROGBITS )
      {
        //kprintf("mapping PROGBITS 0x%x->0x%x\n", (sheader.addr + offset), ((addr_t)img + sheader.offset + offset));

        if( !pte.present )
        {
          *(u32 *)&pte = ((u32)img + sheader.offset + offset) | PAGING_USER |
              (sheader.flags & SHF_WRITE ? PAGING_RW : PAGING_RO) | PAGING_PRES;

          if(writePmapEntry((pde.base << 12), PTE_INDEX(sheader.addr+offset), &pte) !=0)
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

          *(u32 *)&pte = page | PAGING_USER | (sheader.flags & SHF_WRITE ? PAGING_RW
              : PAGING_RO) | PAGING_PRES;

          if(writePmapEntry((pde.base << 12), PTE_INDEX(sheader.addr+offset), &pte) !=0)
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

    pde.base = (u32)stackPTab >> 12;
    pde.rwPriv = pde.usPriv = pde.present = 1;

    pte.base = (u32)stackPage >> 12;
    pte.rwPriv = pte.usPriv = pte.present = 1;

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
      pdePtr->base = (u32)phys >> 12;
      pdePtr->rwPriv = 1;
      pdePtr->usPriv = 0;
      pdePtr->present = 1;
    }
  }

  memset(tcbTable, 0, (int)&kTcbTableSize);

  for(int i=LOWEST_PRIORITY; i <= HIGHEST_PRIORITY; i++)
    queueInit(&runQueues[i]);

  queueInit(&timerQueue);
  pendingMessageBuffer = (pem_t *)freePageStackTop;

  for(addr_t addr=(addr_t)pendingMessageBuffer; addr < (addr_t)(pendingMessageBuffer + MAX_THREADS); addr += PAGE_SIZE)
  {
    pte_t *pte = ADDR_TO_PTE(addr);

    if(!pte->present)
      kMapPage(addr, allocPageFrame(), PAGING_SUPERVISOR | PAGING_RW);
  }
//malloc(sizeof(pem_t)*MAX_THREADS);
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
  char *initServerStrPtr = NULL, *initServerStrEnd=NULL;
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
    freePageFrame((paddr_t)(ADDR_TO_PTE(addr)->base << 12));

  // XXX: Release any pages tables used for discardable sections

  // Release the pages for the TSS IO permission bitmap

  pte = ADDR_TO_PTE((addr_t)EXT_PTR(ioPermBitmap));

  freePageFrame((paddr_t)(pte->base << 12));

  pte = ADDR_TO_PTE((addr_t)EXT_PTR(ioPermBitmap) + PAGE_SIZE);

  freePageFrame((paddr_t)(pte->base << 12));

  freePageFrame((paddr_t)k1To1PageTable);
  freePageFrame((paddr_t)(bootStackTop-PAGE_SIZE));

  *(dword *)EXT_PTR(tssEsp0) = (dword)kernelStackTop;

  schedule();

  for(addr_t addr=(addr_t)0x0000; addr < (addr_t)largePageSize; addr += PAGE_SIZE)
  {
    if(addr == 0x90000)
      addr = (addr_t)0xA0000;

#if DEBUG

    if(addr == 0xB8000)
    {
      addr = (addr_t)0xC0000;
      continue;
    }
#endif /* DEBUG */

    pte = ADDR_TO_PTE(addr);

    if(pte->present)
    {
      pte->present = 0;
      invalidatePage(addr);
    }
  }


  switchContext( initServerThread->rootPageMap, initServerThread->execState );
  stopInit("Error: Context switch failed.");
}
