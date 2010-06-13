#include <elf.h>

#include <kernel/thread.h>
#include <kernel/debug.h>
#include <kernel/mm.h>
#include <kernel/schedule.h>
#include <kernel/cpuid.h>
#include <kernel/paging.h>
#include <kernel/memory.h>
#include <kernel/rtc.h>

#include <oslib.h>
#include <os/io.h>
#include <os/variables.h>

#define MAX_MODULES 50

#define DISC_CODE(X) \
  X __attribute__((section(".dtext")));

#define  DISC_DATA(X)  \
  extern X __attribute__((section(".ddata")));  X

#define INIT_SRV_FLAG	"initsrv="

void contextSwitch( void *addrSpace, void *stack ) __attribute( (section(".dtext")) );

inline void contextSwitch( void *addrSpace, void *stack )
{
   __asm__ __volatile__( \
    "mov %0, %%ecx\n" \
    "mov %%cr3, %%edx\n" \
    "cmp %%edx, %%ecx\n" \
    "jz   __skip\n" \
    "mov  %%ecx, %%cr3\n" \
    "__skip: mov    %1, %%esp\n" \
    "popa\n" \
    "popw  %%es\n" \
    "popw  %%ds\n" \
    "add   $8, %%esp\n" \
    "iret\n" :: "m"(addrSpace), \
    "m"(stack) : "edx", "ecx", "esp" \
    );
}

extern TCB *idleThread;

DISC_DATA( u32 kernelEnd );
DISC_CODE(int load_elf_exec( addr_t img, tid_t exHandler, addr_t addrSpace, addr_t uStack ));
DISC_CODE(bool isValidElfExe( addr_t img ));
DISC_CODE(size_t calcKernelSize( multiboot_info_t *info ));
DISC_CODE(addr_t getModuleStart( multiboot_info_t *info ));
DISC_CODE(size_t getTotalModuleSize( multiboot_info_t *info ));
DISC_CODE(addr_t getModuleEndAddr( multiboot_info_t *info ));
DISC_CODE(void initInterrupts( void ));
DISC_CODE(void initScheduler( addr_t addrSpace ));
DISC_CODE(void initTimer( void ));
DISC_CODE(int initMemory( multiboot_info_t *info ));
DISC_CODE(void initGDT( void ));
DISC_CODE(int add_gdt_entry(int sel, dword base, dword limit, int flags));
DISC_CODE(int initTables( addr_t start, int numThreads, int numRunQueues ));
DISC_CODE(void initTSS(void));
DISC_CODE(void stopInit(char *msg));
DISC_CODE(int initMsgSystem(void));
DISC_CODE(void init2(void));
DISC_CODE(void init(multiboot_info_t *info));
DISC_CODE(void initPIC( void ));
DISC_CODE(int memcmp(const char *s1, const char *s2, register size_t n));
DISC_CODE(int strncmp(const char *str1, const char *str2, size_t num));
DISC_CODE(char *strstr(const char *str, const char *substr));
DISC_CODE(int clearPhysPage(void *phys));
DISC_CODE(void showCPU_Features(void));
DISC_CODE(void showGrubSupport(multiboot_info_t *));
DISC_CODE(void init_clock());
DISC_CODE(unsigned bcd2bin(unsigned short num));
DISC_CODE(unsigned long long mktime(int year, int month, int day, int hour, 
                          int minute, int second));
DISC_DATA( addr_t init_server_img );
DISC_DATA( int numBootMods );
DISC_DATA( module_t kBootModules[MAX_MODULES] );
DISC_DATA(size_t totalPhysMem);
DISC_DATA(addr_t startPhysPages);
DISC_DATA(uint32 totalPages);
DISC_DATA(uint32 totalKSize);
DISC_DATA(multiboot_info_t *grubBootInfo);
DISC_DATA(addr_t modulesStart);
DISC_DATA(size_t totalModulesSize);

struct SectionInfo
{
  unsigned textStart, textLength;
  unsigned dataStart, dataLength;
  unsigned bssStart, bssLength;
};

struct BootInfo
{
  unsigned short num_mods;
  unsigned short num_mem_areas;
} __PACKED__;

struct MemoryArea
{
  unsigned long base;
  unsigned long length;
} __PACKED__;

struct BootModule
{
  unsigned long mod_start;
  unsigned long mod_end;
// Name goes here
} __PACKED__;

DISC_DATA(struct BootInfo boot_info);
DISC_DATA(struct MemoryArea mem_area[20]);

int clearPhysPage( void *phys )
{
//  pte_t *pte;
  assert( phys != (void *)NULL_PADDR );

  if( phys == (void *)NULL_PADDR )
    return -1;

  if( mapTemp( phys ) == -1 )
    return -1;

  memset( (void *)(TEMP_PAGEADDR), 0, PAGE_SIZE );

  unmapTemp();
/*
  *(unsigned *)pte = 0;
*/
  return 0;
}

int memcmp(const char *s1, const char *s2, register size_t n)
{
  if( n == 0 || s1 && !s2 )
    return 0;

  if( !s1 )
    return -*s2

  if( !s2 )
    return *s1;

  for(; n && *s1 == *s2; n--, s1++, s2++);

  if( !n )
    return 0;
  else
    return *s1 - *s2;
}

int strncmp( const char *str1, const char *str2, size_t num )
{
  while( num && *str1 == *str2 && *str1 != '\0' )
  {
    str1++;
    str2++;
    num--;
  }

  if( !num )
    return 0;
  else
    return *str1 - *str2;
}

char *strstr( const char *str, const char *substr )
{
  register int i;

  while( *str )
  {
    i = 0;

    while( str[i] == substr[i] )
      i++;

    if( substr[i] == '\0' )
      return (char *)str;

    str++;
  }

  if( *substr == *str )
    return (char *)str;
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

int add_gdt_entry(int sel, dword base, dword limit, int flags)
{
  int rsel = sel;

  kprintf("GDT: Sel- 0x%x Base: 0x%x Limit: 0x%x Flags: 0x%x\n", sel, base, limit, flags);

  struct gdt_entry *entry = (struct gdt_entry *)KERNEL_GDT;

  if((sel < 0))
    return 0;

  entry += (sel / 8);

  entry->limit1 = (word)(limit & 0xFFFF);
  entry->limit2 = (byte)((limit >> 16) & 0x0F);
  entry->base1 = (word)(base & 0xFFFF);
  entry->base2 = (byte)((base >> 16) & 0xFF);
  entry->base3 = (byte)((base >> 24) & 0xFF);
  entry->flags1 = (byte)(flags & 0xFF);
  entry->flags2 = (byte)((flags >> 8) & 0x0F);

  return rsel;
}

void initTSS(void)
{
  struct TSS_Struct *tss = (struct TSS_Struct *)(PHYSMEM_START + KERNEL_TSS);

  tss->ioMap = IOMAP_LAZY_OFFSET;
  tss->ss0 = KDATA;
  tssEsp0 = (unsigned *)&tss->esp0;
  tssIOBitmap = (void *)TSS_IO_PERM_BMP;

//  kprintf("TSS: IO Map: 0x%x SS: 0x%x Esp0: 0x%x\n", tss->ioMap, tss->ss0, tssEsp0);
//  memset( (void *)(TSS_IO_PERM_BMP + PHYSMEM_START), 0xFF, 8192 );
}

void initGDT(void)
{
  initTSS();

  add_gdt_entry( 0, 0, 0, 0 );
  add_gdt_entry( KCODE, 0, 0xFFFFF, G_GRANULARITY |
      G_BIG | G_RDONLY | G_CODESEG | G_DPL0
    | G_PRESENT );
  add_gdt_entry( KDATA, 0, 0xFFFFF, G_GRANULARITY | G_BIG |
      G_RDWR | G_DATASEG | G_DPL0 | G_PRESENT );

  add_gdt_entry( UCODE, 0, 0xFFFFF, G_PRESENT | G_GRANULARITY |
      G_BIG | /*G_RDONLY*/G_RDWR | G_CODESEG | G_DPL3 );
  add_gdt_entry( UDATA, 0, 0xFFFFF, G_PRESENT | G_GRANULARITY |
      G_BIG | G_RDWR | G_DATASEG | G_DPL3 );
  add_gdt_entry( TSS, (dword)&_kernelTSS, 0x2068, 0x89 );
}

void stopInit(char *msg)
{
  kprintf("Init failed: %s\nSystem halted.", msg);
  disableInt();
  asm("hlt\n");
  while(1);
}

/* Warning: This is fragile code! Any changes to this function or to the
   functions that are called by this function may break the entire
   kernel. */

int initMemory( multiboot_info_t *info )
{
  int bitmapSize;
  int numEntries;
  u32 addr;
  int size;
  pde_t *pde;

  totalPhysMem = ( info->mem_upper + info->mem_lower ) << 10;

  kprintf("Total Memory: 0x%x. %d pages. ", totalPhysMem, totalPhysMem / PAGE_SIZE );

  if( totalPhysMem < (8 << 20) )
    stopInit("Not enough memory!");

  totalPages = totalPhysMem / PAGE_SIZE;

  kernelAddrSpace = ( pdir_t * )INIT_PDIR;

  kprintf("Kernel AddrSpace: 0x%x\n", (unsigned)kernelAddrSpace); 

  if( calcKernelSize( info ) > PAGETAB )
    stopInit("Kernel is too large!(> 4194304 bytes)");

  totalKSize = (u32)PAGE_ALIGN(getModuleEndAddr(info)) - (u32)&kCode;
  kernelEnd = (u32)PAGE_ALIGN(getModuleEndAddr(info));

  kprintf("Total Kernel Size: 0x%x\n", totalKSize);

  size = initTables( (void *)kernelEnd, maxThreads, maxRunQueues );

  if( size == -1 )
    stopInit("Unable to initialize kernel data structure tables.\n");

  totalKSize += size;
  kernelEnd += size;

  totalKSize = (u32)PAGE_ALIGN((addr_t)totalKSize);
  kernelEnd = (u32)PAGE_ALIGN((addr_t)kernelEnd);

  kprintf("Kernel End: 0x%x\n", kernelEnd);

  initGDT();

  asm __volatile__( "mov  %cr4, %eax\n" \
      "or   $0x80, %eax\n" \
      "mov  %eax, %cr4\n" );

  asm __volatile__( "mov  %cr0, %eax\n" \
      "or   $0x00010000, %eax\n" \
      "mov  %eax, %cr0\n" );

  loadGDT();

//  kprintf("Unmapping first page table\n");

  /* Unmap the first page table */

//  pde = ADDR_TO_PDE((addr_t)0x00);
//  memset( pde, 0, sizeof(*pde) );

  return 0;
}


#define UNIX_EPOCH          1970
#define CENTURY_START       2000

static int is_leap( int year )
{
  return ( ((year % 4) == 0 && (year % 100) != 0) || 
      (year % 400) == 0 );
}
                          
unsigned bcd2bin(unsigned short num)
{
  return (num & 0x0F) + 10 * ((num & 0xF0) >> 4) + 100 * ((num & 0xF00) >> 8) 
    + 1000 * ((num & 0xF000) >> 12);
}

unsigned long long mktime(int year, int month, int day, int hour, 
                          int minute, int second)
{
  unsigned long long elapsed = 0ull;
  int month_days[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

  kprintf("mktime: %d %d %d %d %d %d\n", year, month, day, hour, minute, second);
  
  elapsed += second * 1024ull;
  elapsed += minute * 1024ull * 60ull;
  elapsed += hour * 1024ull * 60ull * 60ull;
  elapsed += day * 1024ull * 60ull * 60ull * 24ull;
  
  for(int i=0; i < month; i++)
    elapsed += month_days[i] * 1024ull * 60ull * 60ull * 24ull;
    
  if( is_leap(CENTURY_START+year) && month > 1 )
    elapsed += 1024ull * 60ull * 60ull * 24ull;
  
  for(int i=UNIX_EPOCH; i < CENTURY_START + year; i++)
    elapsed += (is_leap(i) ? 366ull : 365ull) * 60ull * 60ull * 24ull * 1024ull;
  
  return elapsed;
}

void init_clock()
{
  int year, month, day, hour, minute, second;
  unsigned long long *time = (unsigned long long *)KERNEL_VAR_PAGE;
  int bcd, _24hr, status_b;
  
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
  day = -1 + (bcd ? bcd2bin(inByte( RTC_DATA )) : inByte( RTC_DATA ));
  
  outByte( RTC_INDEX, RTC_MONTH ); // month
  month = -1 + (bcd ? bcd2bin(inByte( RTC_DATA )) : inByte( RTC_DATA ));

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
  currentThread = idleThread = createThread( (addr_t)idle, addrSpace, NULL, 0 );
  initTimer();
}


void initInterrupts( void )
{
  int i;

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

  for( i = 0x2F; i <= 0xFF; i++ )
    addIDTEntry( ( void *) invalidIntHandler, i, INT_GATE | KCODE );

  addIDTEntry( ( void * ) syscallHandler, 0x40, INT_GATE | KCODE | I_DPL3 );

  initPIC();
  loadIDT();
}

addr_t getModuleEndAddr( multiboot_info_t *info )
{
  if( info->mods_count == 0 )
    return (addr_t)( KERNEL_VSTART + calcKernelSize( info ) );
  else
    return (addr_t)( PHYS_TO_VIRT(getModuleStart( info )) +
  getTotalModuleSize( info ) );
}

size_t getTotalModuleSize( multiboot_info_t *info )
{
  module_t *kmod = (module_t *)info->mods_addr;
  size_t size = 0;

  if( info->mods_count == 0 )
    return 0;

  size = kmod[info->mods_count - 1].mod_end - kmod[0].mod_start;

  return size;
}

addr_t getModuleStart( multiboot_info_t *info )
{
  module_t *kmod = (module_t *)info->mods_addr;

  if( info->mods_count == 0 )
    return NULL;
  else
    return (addr_t)kmod[0].mod_start;
}

size_t calcKernelSize( multiboot_info_t *info )
{
  elf_section_header_table_t table = info->u.elf_sec;
  int numSections = table.num;

  elf_sheader_t *sheader = (elf_sheader_t *)PHYS_TO_VIRT(table.addr);

  return (sheader[numSections - 1].addr + sheader[numSections - 1].size) -
      (unsigned)KERNEL_START;
}

/* Returns the total size of the tables. */

// TODO: This needs to be cleaned up

int initTables( addr_t start, int numThreads, int numRunQueues )
{
  unsigned char *ptr = (unsigned char *)start;
  int i;

  kprintf("Kernel Tables: Start - 0x%x ", start);

  if( numThreads < 1 || numRunQueues < 1 || numRunQueues > 15 )
    return -1;

  tcbTable = (TCB *)ptr;
  memset( tcbTable, 0, numThreads * sizeof( TCB ) );
  ptr += numThreads * sizeof( TCB );

  kprintf("TCB Table: 0x%x\n", tcbTable);

  runQueues = (struct Queue *)ptr;

  kprintf("Run Queues: 0x%x\n", runQueues);

  for(i=0; i < numRunQueues; i++)
    runQueues[i].head = runQueues[i].tail = NULL_TID;

  ptr += numRunQueues * sizeof( struct Queue );

  tcbNodes = (struct NodePointer *)ptr;

  kprintf("TCB Nodes: 0x%x ", tcbNodes);

  for(i=0; i < numThreads; i++)
    tcbNodes[i].next = tcbNodes[i].prev = NULL_TID;

  ptr += sizeof( struct NodePointer );

  sleepQueue.head = NULL_TID;

  return (unsigned)ptr - (unsigned)start;
}

bool isValidElfExe( addr_t img )
{
  elf_header_t *image = ( elf_header_t * ) img;

  if( img == NULL )
    return false;

  if ( memcmp( "\x7F""ELF", image->identifier, 4 ) != 0 )
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
/* Requires physical pages for BSS */

int load_elf_exec( addr_t img, tid_t exHandler, addr_t addrSpace, addr_t uStack )
{
  unsigned phtab_entsize, phtab_count, shtab_count, i, j;
  elf_header_t *image = (elf_header_t *)img;
  elf_pheader_t *pheader;
  elf_sheader_t *sheader;
  addr_t tempAddr;
  TCB *thread;
  void *phys;
  dword priv;
  pde_t pde;
  struct MemoryArea *m_area=NULL;

  if( !isValidElfExe( img ) )
  {
    kprintf("Not a valid ELF executable.\n");
    return -1;
  }

  phtab_entsize = image->phentsize;
  phtab_count = image->phnum;
  shtab_count = image->shnum;

  thread = createThread( (addr_t)image->entry, addrSpace, uStack, exHandler );

  if( thread == NULL )
  {
    kprintf("load_elf_exec(): Couldn't create thread.\n");
    return -1;
  }
       
  /* Program header information is loaded into memory */

  pheader = (elf_pheader_t *)(img + image->phoff);
  sheader = (elf_sheader_t *)(img + image->shoff);

  /* Create the page table before mapping memory */

  *(u32 *)&pde = INIT_SERVER_PTAB | PAGING_RW | PAGING_USER | PAGING_PRES;
  writePDE( (void *)pheader->vaddr, &pde, addrSpace );

  for ( i = 0; i < phtab_count; i++, pheader++ )
  {
    if ( pheader->type == PT_LOAD )
    {
      unsigned memSize = pheader->memsz;
      unsigned fileSize = pheader->filesz;

      kprintf("Vaddr: 0x%x Offset: 0x%x MemSize: 0x%x\n", pheader->vaddr, pheader->offset, memSize);

      for( j=0; j < boot_info.num_mem_areas; j++)
      {
      //  kprintf("Base: 0x%x Length: %d\n", mem_area[j].base, mem_area[j].length);
        if( memSize <= mem_area[j].length * PAGE_SIZE )
        {
          m_area = &mem_area[j];
          kprintf("Base: 0x%x Length: %d pages\n", mem_area[j].base, mem_area[j].length);
//          kprintf("Found!\n");
          break;
        } 
      }

      if( j == boot_info.num_mem_areas || m_area == NULL )
        stopInit("Not enough memory for BSS allocation");

      for ( j = 0; memSize > 0; j++ )
      {
        if ( fileSize == 0 )
        {
          /* Picks a physical address for BSS */
          phys = (void *)m_area->base; // This can be done better
          priv = PAGING_RW;
          clearPhysPage( phys );
          m_area->base += PAGE_SIZE;
          m_area->length--;
        }
        else
        {
          phys = (void *)VIRT_TO_PHYS((pheader->offset + (int)image + j * PAGE_SIZE));
          priv = PAGING_RW; // TODO: Depends on whether it's read/write or read-only
        }
        
        //kprintf("mapping page: vaddr-0x%x phys-0x%x addrSpace-0x%x\n", pheader->vaddr+j*PAGE_SIZE, phys, addrSpace);

        mapPage( (void *)( pheader->vaddr + j * PAGE_SIZE ), phys, priv | PAGING_USER, addrSpace );
  
        if( memSize < PAGE_SIZE )
          memSize = 0;
        else
          memSize -= PAGE_SIZE;

        if( fileSize < PAGE_SIZE )
          fileSize = 0;
        else
          fileSize -= PAGE_SIZE;
      }
    }
  }

  startThread( thread );

  return 0;
}

// TODO: This *really* needs to be cleaned up

/**
    Bootstraps the initial server and passes necessary boot data to it.
*/

void init2( void )
{
  assert( tcbTable->state != DEAD );
  int i;
  char **argv;
  int size = (unsigned)&kBss - (unsigned)&kdCode;
  pte_t pte;
  pde_t pde;
  char *string;
  int *ptr;
//  tid_t tid;
  struct BootInfo *b_info;
  struct MemoryArea *memory_area;
  struct BootModule *boot_mods;
  char *init_server_stack = (char *)(KERNEL_VSTART - PAGE_SIZE);

  /* Each entry in a page directory and page table must be cleared 
     prior to using. */

  clearPhysPage( (void *)INIT_SERVER_PDIR );
  clearPhysPage( (void *)INIT_SERVER_PTAB );
  clearPhysPage( (void *)INIT_SERVER_USTACK_PTAB );
//  clearPhysPage( (void *)INIT_SERVER_USTACK_PAGE ); This shouldn't need clearing

  ptr = (int *)(TEMP_PAGEADDR + PAGE_SIZE);

  kprintf("\n0x%x bytes of discardable code.", (unsigned)&kdData - (unsigned)&kdCode);
  kprintf(" 0x%x bytes of discardable data.\n", (unsigned)&kBss - (unsigned)&kdData);

  if(boot_info.num_mods == 0)
  {
    kprintf("No boot modules found.\nNo initial server to start.\n");
    return;
  }

  /* Add the discarded kernel pages to the available physical memory table for the pager
     to use.  */

  mem_area[boot_info.num_mem_areas].base = (unsigned long)VIRT_TO_PHYS(&kdCode);
  mem_area[boot_info.num_mem_areas].length = (unsigned long)PAGE_ALIGN((addr_t)size) / PAGE_SIZE;

  boot_info.num_mem_areas++;

  kprintf("Discarding: %d bytes of kernel code/data\n", size);

  if( isValidElfExe( (addr_t)init_server_img ) )
  {
    *(u32 *)&pde = INIT_SERVER_USTACK_PTAB | PAGING_RW | PAGING_USER | PAGING_PRES;
    writePDE(init_server_stack, &pde, (void *)INIT_SERVER_PDIR);
    *(u32 *)&pte = INIT_SERVER_USTACK_PAGE | PAGING_RW | PAGING_USER | PAGING_PRES;
    writePTE(init_server_stack, &pte, (void *)INIT_SERVER_PDIR);

    string = (char *)TEMP_PAGEADDR;//(PHYSMEM_START + INIT_SERVER_USTACK_PAGE /*- PAGE_SIZE*/);
    b_info = (struct BootInfo *)(string + 8);
    memory_area = (struct MemoryArea *)(b_info + 1);
    boot_mods = (struct BootModule *)(memory_area + boot_info.num_mem_areas);
    argv = (char **)(boot_mods + boot_info.num_mods);

    /* This is placed here because load_elf_exec() modifies the
       memory_area struct. The modified memory_area struct would
       need to be written to the root server's stack. */

    mapTemp((addr_t)INIT_SERVER_USTACK_PAGE);

    for(i=0; i < boot_info.num_mods; i++)
    {
      boot_mods[i].mod_start = kBootModules[i].mod_start;
      boot_mods[i].mod_end = kBootModules[i].mod_end;
      //kprintHex(kBootModules[i].mod_start);
      //kprintf(" ");
    }

//    mapTemp(INIT_SERVER_USTACK_PAGE);

    *(char **)&argv[6] = (init_server_stack + ((unsigned)argv & 0xFFF));
    *(int *)&argv[5] = 5;
    argv[4] = (char *)INIT_SERVER_PDIR;
    argv[3] = (init_server_stack + ((unsigned)boot_mods & 0xFFF));
    argv[2] = (init_server_stack + ((unsigned)memory_area & 0xFFF));
    argv[1] = (init_server_stack + ((unsigned)b_info & 0xFFF));
    argv[0] = (init_server_stack + ((unsigned)string & 0xFFF));

    /* I don't know why the third '--ptr' needs to be done..., but without it, argc and argv don't work. */

    //kprintf("0x ");kprintHex(init_server_stack);

    *--ptr = (int)(init_server_stack + ((unsigned)&argv[5] & 0xFFF));  // ebp + 8 <- First argument
    --ptr; // ebp + 4 <- The return address 
   
  /* Put the stack at the top of the physical memory area. */
    unmapTemp();

    if ( (init_server_tid=load_elf_exec( init_server_img, 1, 
         (addr_t)INIT_SERVER_PDIR, (init_server_stack + 
         ((unsigned)ptr & 0xFFF)) )) != 0 )
    {
      kprintf("Failed to initialize initial server.\n");
    }

    mapTemp((addr_t)INIT_SERVER_USTACK_PAGE);

    memcpy(string, "initsrv", 8);
    memcpy(b_info, &boot_info, sizeof boot_info);
    memcpy(memory_area, mem_area, sizeof(struct MemoryArea) * boot_info.num_mem_areas);

    *(u32 *)&pte = 0; // What does this do???

    unmapTemp();
  }
  else
    kprintf("Unable to load initial server.\n");
}

void showGrubSupport( multiboot_info_t *info )
{
  if( info->flags & (1 <<0) )
    kprintf("Mem upper/lower supported.\n");
  if( info->flags & (1 <<1) )
    kprintf("Boot device supported.\n");
  if( info->flags & (1 <<2) )
    kprintf("Command line supported.\n");
  if( info->flags & (1 <<3) )
    kprintf("Modules supported.\n");
  if( (info->flags & (1 <<4)) || (info->flags & (1 << 5)) )
    kprintf("Symbol table supported.\n");
  if( info->flags & (1 <<6) )
    kprintf("Memory map supported.\n");
  if( info->flags & (1 <<7) )
    kprintf("Drive info supported.\n");
  if( info->flags & (1 <<8) )
    kprintf("Config table supported.\n");
  if( info->flags & (1 <<9) )
    kprintf("Boot loader name supported.\n");
  if( info->flags & (1 <<10) )
    kprintf("APM table supported.\n");
  if( info->flags & (1 << 11) )
    kprintf("VBE supported.\n");
}

void showCPU_Features(void)
{
  asm __volatile__("cpuid\n" : "=d"(cpuFeatures) : "a"(0x01));

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

// TODO: This could be more organized and cleaner

/**
    Bootstraps the kernel.

    @param info The multiboot structure passed by the bootloader.
*/

void init( multiboot_info_t *info )
{
  memory_map_t *mmap;
  module_t *module;
  unsigned stack;
  int i=0;
  bool init_srv_found=false;
  char *srv_str_ptr = NULL, *srv_str_end=NULL;

#ifdef DEBUG
  init_serial();
  initVideo();
  setVideoLowMem( false );
  clearScreen();
  showGrubSupport(info);
  showCPU_Features();
#endif

  srv_str_ptr = strstr( (char *)(PHYSMEM_START + info->cmdline), INIT_SRV_FLAG );

  if( srv_str_ptr )
  {
    srv_str_ptr += (sizeof( INIT_SRV_FLAG ) - 1);
    srv_str_end = srv_str_ptr;

    while( *srv_str_end != ' ' && *srv_str_end != '\0' )
      srv_str_end++;
  }

  /* Prepare the modules for use later. */

  info->mods_addr += PHYSMEM_START;
  kprintf("Boot info struct: 0x%x\nModules located at 0x%x. %d modules\n", info, info->mods_addr, info->mods_count);
  module = (module_t *)info->mods_addr;
  numBootMods = info->mods_count;

  boot_info.num_mem_areas = boot_info.num_mods = 0;

  info->mmap_addr += PHYSMEM_START;
  mmap = (memory_map_t *)info->mmap_addr;

  if( numBootMods > MAX_MODULES )
    stopInit("Too many modules.\n");
  else
  {    
    for(i=0; i < numBootMods; i++, module++)
    {
      boot_info.num_mods++;

      if( srv_str_ptr && !init_srv_found )
      {
        if( strncmp((char *)(PHYSMEM_START+module->string), srv_str_ptr, 
                    srv_str_end-srv_str_ptr) == 0 )
        {
          init_server_img = PHYS_TO_VIRT(module->mod_start);
        }

        init_srv_found = true;
      }
      else if( i == 0 )
        init_server_img = PHYS_TO_VIRT(module->mod_start);

      kBootModules[i] = *module;
    }
  }

  maxThreads = 1024;
  maxRunQueues = NUM_PRIORITIES;

  kprintf("Max values: %d threads. %d run queues.\n", maxThreads, maxRunQueues);

  if( initMemory( info ) < 0 )
    stopInit("Not enough memory! At least 8 MiB is needed");


  kMapMem( (void *)CLOCK_TICKS, (void *)KERNEL_VAR_PAGE, PAGING_RO | PAGING_USER );

  size_t len;

  /* Determine which portions of memory are free & usable.*/

  if( info->flags & (1 << 6) ) // Is mmap valid?
  {
    addr_t start_addr;

    /* Do memory checks here. */
    for( ; (unsigned)mmap < info->mmap_length+info->mmap_addr; mmap=(memory_map_t *)((unsigned)mmap + mmap->size + sizeof(mmap->size)))
    {
      start_addr = NULL;

      /* Don't touch this. */

      if( mmap->type != 1 )
        continue;

      if( mmap->base_addr_low == 0x00 && (BOOTSTRAP_STACK_TOP) < mmap->base_addr_low + 
          mmap->length_low )
      {
        mmap->base_addr_low = (BOOTSTRAP_STACK_TOP);
        mmap->length_low -= (BOOTSTRAP_STACK_TOP);
        mmap->length_low &= ~(PAGE_SIZE - 1);
      }

      /* Split if the region includes the kernel */

      if( mmap->base_addr_low <= KERNEL_START && 
          mmap->length_low > KERNEL_START - mmap->base_addr_low )
      {
        len = mmap->length_low - totalKSize - (KERNEL_START - mmap->base_addr_low);
        mmap->length_low = (KERNEL_START - mmap->base_addr_low);
        start_addr = (addr_t)(KERNEL_START + totalKSize);

        //kprintHex( (unsigned)start_addr + len );
      }

      if( mmap->length_low != 0)
      {
        mem_area[boot_info.num_mem_areas].base = mmap->base_addr_low;
        mem_area[boot_info.num_mem_areas].length = (u32)PAGE_ALIGN((addr_t)mmap->length_low) / PAGE_SIZE;
        kprintf("Base low: 0x%x. Length: %d pages\n", mem_area[boot_info.num_mem_areas].base, mem_area[boot_info.num_mem_areas].length);
        boot_info.num_mem_areas++;
      }

//      kprintf("Base low: 0x%x. Length: 0x%x\n", mem_area[boot_info.num_mem_areas].base, mem_area[boot_info.num_mem_areas].length);
//      boot_info.num_mem_areas++;

      if( start_addr != NULL && len / PAGE_SIZE > 0 )
      {
        mem_area[boot_info.num_mem_areas].base = (unsigned long)start_addr;
        mem_area[boot_info.num_mem_areas].length = len / PAGE_SIZE;
        kprintf("Base low: 0x%x. Length: %d pages\n", mem_area[boot_info.num_mem_areas].base, mem_area[boot_info.num_mem_areas].length);
        boot_info.num_mem_areas++;
      }
    }
  }

  initInterrupts();
//  enable_apic();
//  init_apic_timer();
  initScheduler( (addr_t)kernelAddrSpace );
  currentThread->state = RUNNING;

  /* !!! 1st thread doesn't have a valid tss IO bitmap !!! */

  if( currentThread == NULL )
    stopInit("Failed to initialize the idle thread");

//  *tssEsp0 = (unsigned)(&currentThread->regs + 1);

//  __asm__ volatile("wbinvd\n");

  if( GET_TID(currentThread) == 0 )
  {
    stack = V_IDLE_STACK_TOP - sizeof(Registers);
    *tssEsp0 = (unsigned)V_IDLE_STACK_TOP;
  }
  else
  {
    stack = (unsigned)&currentThread->regs;
    *tssEsp0 = (unsigned)(&currentThread->regs + 1);
  }

  kprintf("Stack: 0x%x\n", stack);

  assert( tcbTable->state != DEAD );

  init_clock();

  contextSwitch( currentThread->addrSpace, (void *)stack );
  stopInit("Unable to start operating system");
}
