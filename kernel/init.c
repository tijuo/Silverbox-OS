#include <elf.h>

#include <kernel/thread.h>
#include <kernel/debug.h>
#include <kernel/mm.h>
#include <kernel/schedule.h>
#include <kernel/cpuid.h>
#include <kernel/paging.h>
#include <kernel/memory.h>
#include <kernel/mm.h>
#include <kernel/rtc.h>
#include <kernel/pit.h>
#include <kernel/pic.h>
#include <oslib.h>

#include <kernel/io.h>
#include <os/variables.h>

#define FREE_PAGE_PTR_START     0x100000u

#define DISC_CODE(X) \
  X __attribute__((section(".dtext")))

#define  DISC_DATA(X)  \
  X __attribute__((section(".ddata")))

#define INIT_SRV_FLAG	"initsrv="

static inline void contextSwitch( cr3_t addrSpace, addr_t stack ) __attribute__((section(".dtext")));

static inline void contextSwitch( cr3_t addrSpace, addr_t stack )
{
   __asm__ __volatile__(
    "mov %%edx, %%esp\n"
    "mov %%cr3, %%eax\n"
    "cmp %%eax, %%ecx\n"
    "je   __load_regs\n"
    "mov  %%ecx, %%cr3\n"
    "__load_regs:\n"
    "popa\n"
    "pop  %%es\n"
    "pop  %%ds\n"
    "add   $8, %%esp\n"
    "iret\n" :: "ecx"(*(dword *)&addrSpace),
    "edx"((dword)stack) : "%eax");
}

extern TCB *idleThread;

static TCB *DISC_CODE(load_elf_exec( addr_t, TCB *, addr_t, addr_t ));
static bool DISC_CODE(isValidElfExe( addr_t img ));
static void DISC_CODE(initInterrupts( void ));
static void DISC_CODE(initScheduler( addr_t ));
static void DISC_CODE(initTimer( void ));
static int DISC_CODE(initMemory( multiboot_info_t *info ));
//int DISC_CODE(add_gdt_entry(int sel, dword base, dword limit, int flags));
static void DISC_CODE(setupGDT(void));
static void DISC_CODE(stopInit(const char *));
static void DISC_CODE(init2(multiboot_info_t * restrict));
void DISC_CODE(init(multiboot_info_t * restrict));
static void DISC_CODE(initPIC( void ));
static int DISC_CODE(memcmp(const char *s1, const char *s2, register size_t n));
static int DISC_CODE(strncmp(const char * restrict, const char * restrict, size_t num));
//static size_t DISC_CODE(strlen(const char *s));
static char *DISC_CODE(strstr(const char * restrict, const char * restrict));
static char *DISC_CODE(strchr(const char * restrict, int));
static int DISC_CODE(clearPhysPage(addr_t phys));
static void DISC_CODE(showCPU_Features(void));
static void DISC_CODE(showMBInfoFlags(multiboot_info_t * restrict));
static void DISC_CODE(init_clock(void));
static void DISC_CODE(initStructures(void));
static unsigned DISC_CODE(bcd2bin(unsigned num));
static unsigned long long DISC_CODE(mktime(unsigned int year, unsigned int month, unsigned int day, unsigned int hour,
                          unsigned int minute, unsigned int second));
static addr_t DISC_DATA(init_server_img);
static addr_t DISC_CODE(alloc_page(void));

addr_t DISC_DATA(free_page_ptr) = (addr_t)FREE_PAGE_PTR_START;
//static void DISC_CODE( readPhysMem(addr_t address, addr_t buffer, size_t len) );

extern void invalidate_page( addr_t );
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
int clearPhysPage( addr_t phys )
{
  assert( phys != NULL_PADDR );

  if( mapTemp( phys ) == -1 )
    return -1;

  memset( (void *)TEMP_PAGEADDR, 0, PAGE_SIZE );

  unmapTemp();
  return 0;
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
  size_t len;
  pde_t *pdePtr;
  addr_t addr;

  kprintf("Total Memory: 0x%x. %d pages. ", totalPhysMem, totalPhysMem / PAGE_SIZE );

  if( totalPhysMem < (8 << 20) )
    stopInit("Not enough memory!");

  kprintf("Kernel AddrSpace: 0x%x\n", (unsigned)initKrnlPDir);

  for( addr=(addr_t)EXT_PTR(kCode); addr < (addr_t)EXT_PTR(kData); addr += PAGE_SIZE )
  {
    ptePtr = ADDR_TO_PTE( addr );

    ptePtr->rwPriv = 0;
    invalidate_page( addr );
  }

  addr = (addr_t)EXT_PTR(kPhysStart);
  len = (size_t)EXT_PTR(kSize);

  do
  {
    pdePtr = ADDR_TO_PDE(addr);
    pdePtr->present = 0;
    addr += TABLE_SIZE;
    len -= (len > TABLE_SIZE ? TABLE_SIZE : len);
  } while(len);

  setupGDT();

  return 0;
}

#define UNIX_EPOCH          1970u
#define CENTURY_START       2000u

static int is_leap( unsigned int year )
{
  return ( ((year % 4u) == 0 && (year % 100u) != 0) ||
      (year % 400u) == 0 );
}

unsigned int bcd2bin(unsigned int num)
{
  return (num & 0x0Fu) + 10 * ((num & 0xF0u) >> 4) + 100 * ((num & 0xF00u) >> 8)
    + 1000 * ((num & 0xF000u) >> 12);
}

unsigned long long mktime(unsigned int year, unsigned int month,
    unsigned int day, unsigned int hour, unsigned int minute, unsigned int second)
{
  unsigned long long elapsed = 0ull;
  unsigned int month_days[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
  unsigned int i;

  kprintf("mktime: %d %d %d %d %d %d\n", year, month, day, hour, minute, second);

  elapsed += second * 1024ull;
  elapsed += minute * 1024ull * 60ull;
  elapsed += hour * 1024ull * 60ull * 60ull;
  elapsed += day * 1024ull * 60ull * 60ull * 24ull;

  for(i=0; i < month; i++)
    elapsed += month_days[i] * 1024ull * 60ull * 60ull * 24ull;

  if( is_leap(CENTURY_START+year) && month > 1u )
    elapsed += 1024ull * 60ull * 60ull * 24ull;

  for(i=UNIX_EPOCH; i < CENTURY_START + year; i++)
    elapsed += (is_leap(i) ? 366ull : 365ull) * 60ull * 60ull * 24ull * 1024ull;

  return elapsed;
}

void init_clock()
{
  unsigned int year, month, day, hour, minute, second;
  unsigned long long *time = (unsigned long long *)KERNEL_CLOCK;
  unsigned int bcd, _24hr;
  unsigned char status_b;

  addIDTEntry( irq8Handler, IRQ8, INT_GATE | KCODE );

  outByte( RTC_INDEX, RTC_STATUS_B );
  status_b = inByte( RTC_DATA );

  if( status_b & RTC_BINARY )
    bcd = 0;
  else
    bcd = 1;

  if( status_b & RTC_24_HR )
    _24hr = 1;
  else
    _24hr = 0;

  outByte( RTC_INDEX, RTC_STATUS_B );
  outByte( RTC_DATA, status_b | RTC_PERIODIC );

  kprintf("Enabling IRQ 8\n");
  enableIRQ(8);

  outByte( RTC_INDEX, RTC_SECOND ); // second
  second = (bcd ? bcd2bin(inByte( RTC_DATA )) : inByte( RTC_DATA ));

  outByte( RTC_INDEX, RTC_MINUTE ); // minute
  minute = (bcd ? bcd2bin(inByte( RTC_DATA )) : inByte( RTC_DATA ));

  outByte( RTC_INDEX, RTC_HOUR ); // hour
  hour = (bcd ? bcd2bin(inByte( RTC_DATA )) : inByte( RTC_DATA ));

  if( !_24hr )
  {
    hour--;

    if( hour & 0x80 )
      hour = 12 + (hour & 0x7F);
    else
      hour = (hour & 0x7F);
  }

  outByte( RTC_INDEX, RTC_DAY ); // day
  day = (bcd ? bcd2bin(inByte( RTC_DATA )) : inByte( RTC_DATA )) - 1;

  outByte( RTC_INDEX, RTC_MONTH ); // month
  month = (bcd ? bcd2bin(inByte( RTC_DATA )) : inByte( RTC_DATA )) - 1;

  outByte( RTC_INDEX, RTC_YEAR ); // century year (00-99)
  year = (bcd ? bcd2bin(inByte( RTC_DATA )) : inByte( RTC_DATA ));

  *time = mktime(year, month, day, hour, minute, second);
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

void initScheduler( addr_t addrSpace )
{
  currentThread = idleThread = createThread( (addr_t)idle, addrSpace,
    (addr_t)EXT_PTR(idleStack) + idleStackLen, NULL_TID );

  idleThread->priority = LOWEST_PRIORITY;
  idleThread->quantaLeft = LOWEST_PRIORITY + 1;
  idleThread->threadState = RUNNING;

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
  addIDTEntry( ( void * ) invalidIntHandler, 0x28, INT_GATE | KCODE ) ;
  //addIDTEntry( ( void * ) irq8Handler, 0x28, INT_GATE | KCODE ) ;
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

addr_t alloc_page(void)
{
  free_page_ptr += PAGE_SIZE;
  return free_page_ptr - PAGE_SIZE;
}

TCB *load_elf_exec( addr_t img, TCB * restrict exHandler, addr_t addrSpace, addr_t uStack )
{
  elf_header_t image;
  elf_sheader_t sheader;
  TCB *thread;
  addr_t page;
  pde_t pde;
  pte_t pte;
  size_t i, offset;

  peek( img, (addr_t)&image, sizeof image );
  pte.present = 0;

  #if DEBUG
    int result;
  #endif /* DEBUG */

  if( !isValidElfExe( (addr_t)&image ) )
  {
    kprintf("Not a valid ELF executable.\n");
    return NULL;
  }

  thread = createThread( (addr_t)image.entry, addrSpace, uStack, exHandler );

  if( thread == NULL )
  {
    kprintf("load_elf_exec(): Couldn't create thread.\n");
    return NULL;
  }

  /* Create the page table before mapping memory */

  for( i=0; i < image.shnum; i++ )
  {
    peek( (img + image.shoff + i * image.shentsize), (addr_t)&sheader, sizeof sheader );

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
        page = alloc_page();
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
        kprintf("mapping PROGBITS 0x%x->0x%x\n", (sheader.addr + offset), ((addr_t)img + sheader.offset + offset));

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
          page = alloc_page();
          clearPhysPage( page );
          kprintf("mapping NOBITS 0x%x->0x%x\n", (sheader.addr + offset), page);

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

// TODO: This *really* needs to be cleaned up

/**
    Bootstraps the initial server and passes necessary boot data to it.
*/

void init2( multiboot_info_t * restrict mb_boot_info )
{
  pmap_t pmap, pmap2;
  addr_t page;
  unsigned int *ptr = (unsigned int *)(TEMP_PAGEADDR + PAGE_SIZE);
  addr_t init_server_stack = KERNEL_VSTART - PAGE_SIZE;
  addr_t uStackPage, initServerPDir;
  elf_header_t elf_header;

  peek( init_server_img, (addr_t)&elf_header, sizeof elf_header );

  if( isValidElfExe( (addr_t)&elf_header ) )
  {
    /* Allocate memory for the page directory and stack including any necessary page tables and
       start the initial server. */

    initServerPDir = alloc_page();

    clearPhysPage(initServerPDir);

    if( (init_server=load_elf_exec(init_server_img, NULL,
         initServerPDir, init_server_stack + (((addr_t)ptr - 3) & 0xFFFu)) ) == NULL )
    {
      kprintf("Failed to initialize initial server.\n");
      return;
    }

    page = alloc_page();
    clearPhysPage(page);

    *(u32 *)&pmap = (u32)page | PAGING_RW | PAGING_USER | PAGING_PRES;
    //writePDE(init_server_stack, &pmap, initServerPDir);

    writePmapEntry(initServerPDir, PDE_INDEX(init_server_stack), &pmap);

    uStackPage = alloc_page();
    clearPhysPage(uStackPage);

    *(u32 *)&pmap2 = (u32)uStackPage | PAGING_RW | PAGING_USER | PAGING_PRES;
    writePmapEntry((pmap.base << 12), PTE_INDEX(init_server_stack), &pmap2);

    //writePTE(init_server_stack, &pmap, initServerPDir);

    /* Push the bootstrap arguments onto the stack. */

    mapTemp(uStackPage);

    *--ptr = (unsigned int)&kBss - (unsigned int)&kdCode; // Arg 5
    *--ptr = (unsigned int)&kdCode; // Arg 4
    *--ptr = (unsigned int)free_page_ptr - FREE_PAGE_PTR_START; // Arg 3
    *--ptr = FREE_PAGE_PTR_START; // Arg 2
    *--ptr = (unsigned int)mb_boot_info; // Arg 1

    unmapTemp();
    init_server->privileged = 1;
    init_server->pager = 1;
    startThread(init_server);
  }
  else
  {
    kprintf("Unable to load initial server.\n");
  }

 // kprintf("\n0x%x bytes of discardable code.", (unsigned)&kdData - (unsigned)&kdCode);
 // kprintf(" 0x%x bytes of discardable data.\n", (unsigned)&kBss - (unsigned)&kdData);

 // kprintf("Discarding %d bytes of kernel code/data\n", (unsigned)&kBss - (unsigned)&kdCode);

  /* Get rid of the code and data that will never be used again. */

//  for( addr_t addr=(addr_t)&kdCode; addr < (addr_t)&kBss; addr += PAGE_SIZE )
//    kUnmapPage( addr );
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

void showMBInfoFlags( multiboot_info_t * restrict info )
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

void initStructures(void)
{
  size_t i=0;

  for(i=INITIAL_TID+1; i < MAX_THREADS; i++)
  {
    tcbTable[i].queuePrev = &tcbTable[i-1];
    tcbTable[i-1].queueNext = &tcbTable[i];
  }

  freeThreadQueue.head = &tcbTable[INITIAL_TID];
  freeThreadQueue.tail = &tcbTable[MAX_THREADS-1];
}

/**
    Bootstraps the kernel.

    @param info The multiboot structure passed by the bootloader.
*/

void init( multiboot_info_t * restrict info )
{
//  memory_map_t *mmap;
  module_t *module;
  addr_t stack;
  unsigned int i=0;
  bool init_srv_found=false;
  char *srv_str_ptr = NULL, *srv_str_end=NULL;

#ifdef DEBUG
  init_serial();
  initVideo();
  setVideoLowMem( true );
  clearScreen();
  showMBInfoFlags(info);
  showCPU_Features();
#endif

  srv_str_ptr = strstr( (char *)info->cmdline, INIT_SRV_FLAG );

  /* Locate the initial server string (if it exists) */

  if( srv_str_ptr )
  {
    srv_str_ptr += (sizeof( INIT_SRV_FLAG ) - 1);
    srv_str_end = strchr(srv_str_ptr, ' ');

    if( !srv_str_end )
      srv_str_end = strchr(srv_str_ptr, '\0');
  }
  else
  {
    kprintf("Initial server not specified.\n");

   if( !info->mods_count )
     stopInit("No boot modules found.\nNo initial servers to start.");
  }

  if( info->flags & 1 )
  {
    kprintf("Lower Memory: %d B Upper Memory: %d B\n", info->mem_lower << 10, info->mem_upper << 10);
  }

  kprintf("Boot info struct: 0x%x\nModules located at 0x%x. %d modules\n",
    info, info->mods_addr, info->mods_count);
  module = (module_t *)info->mods_addr;

  /* Copy the boot modules and locate the initial server. */

  if( !srv_str_ptr ) // If no initial server was specified, assume that the first module is the initial server
    init_server_img = (addr_t)module->mod_start;
  else
  {
    for(i=info->mods_count; i; i--, module++)
    {
      if( strncmp((char *)module->string, srv_str_ptr,
                      srv_str_end-srv_str_ptr) == 0 )
      {
        init_server_img = (addr_t)module->mod_start;
        init_srv_found = true;
        break;
      }
    }
  }

  if( !init_srv_found )
    stopInit("Can't find initial server.");

  kprintf("%d threads. %d run queues.\n", MAX_THREADS, NUM_RUN_QUEUES);

  /* Initialize memory */

  if( initMemory( info ) < 0 )
    stopInit("Not enough memory! At least 8 MiB is needed.");

  initStructures();

  // Map the kernel variable page (there should be a rw/supervisor page and a r/o user page that points to kernelVars)
  // That way, the user can read the kernel variables page while the kernel can write to it while in kernel mode

  kMapPage( (addr_t)KERNEL_VARIABLES, (addr_t)kernelVars,
    PAGING_RW | PAGING_USER ); // this should be mapped as read-only

  initInterrupts();
//  enable_apic();
//  init_apic_timer();
  initScheduler( (addr_t)initKrnlPDir );

  if( currentThread == NULL )
    stopInit("Unable to initialize the idle thread.");

  if( GET_TID(currentThread) == IDLE_TID )
    stack = (addr_t)EXT_PTR(idleStack) + idleStackLen - (sizeof(ExecutionState) - 8);
  else
    stack = (addr_t)&currentThread->execState;

  assert( currentThread->threadState != DEAD );

  init_clock();
#if 0
  testATA();
#endif /* DEBUG */
  init2(info);

  contextSwitch( currentThread->cr3, stack );
  stopInit("Unable to start operating system.");
}

