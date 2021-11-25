#include <elf.h>

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
#include "init.h"

#define KERNEL_IDT_LEN	(64 * sizeof(struct IdtEntry))

DISC_DATA const char *MB_INFO_NAMES[] = {
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

DISC_DATA const char *FEATURES_STR[32] = {
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

DISC_DATA const char *FEATURES_STR2[32] = {
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

DISC_DATA const char *PROCESSOR_TYPE[4] = {
  "OEM Processor",
  "Intel Overdrive Processor",
  "Dual Processor",
  "???"
};

DISC_CODE static void disableIRQ(unsigned int irq);

DISC_DATA ALIGNED(PAGE_SIZE) uint8_t kBootStack[4 * PAGE_SIZE];
DISC_DATA void *kBootStackTop = kBootStack + sizeof kBootStack;

extern idt_entry_t kernelIDT[NUM_EXCEPTIONS + NUM_IRQS];

extern uint8_t *kernelStackTop;

DISC_CODE static void initInterrupts(void);
//DISC_CODE static void initTimer( void ));

DISC_CODE static void stopInit(const char*);
DISC_CODE void init(multiboot_info_t*);
//DISC_CODE static void initPIC( void ));

DISC_CODE void addIDTEntry(void (*f)(void), unsigned int entryNum,
                           unsigned int dpl);
DISC_CODE void loadIDT(void);

DISC_DATA static void (*CPU_EX_ISRS[NUM_EXCEPTIONS])(
    void) = {
      cpuEx0Handler, cpuEx1Handler, cpuEx2Handler, cpuEx3Handler, cpuEx4Handler, cpuEx5Handler,
      cpuEx6Handler, cpuEx7Handler, cpuEx8Handler, cpuEx9Handler, cpuEx10Handler, cpuEx11Handler,
      cpuEx12Handler, cpuEx13Handler, cpuEx14Handler, cpuEx15Handler, cpuEx16Handler, cpuEx17Handler,
      cpuEx18Handler, cpuEx19Handler, cpuEx20Handler, cpuEx21Handler, cpuEx22Handler, cpuEx23Handler,
      cpuEx24Handler, cpuEx25Handler, cpuEx26Handler, cpuEx27Handler, cpuEx28Handler, cpuEx29Handler,
      cpuEx30Handler, cpuEx31Handler
};

DISC_DATA static void (*IRQ_ISRS[NUM_IRQS])(
    void) = {
      irq0Handler, irq1Handler, irq2Handler, irq3Handler, irq4Handler, irq5Handler,
      irq6Handler, irq7Handler, irq8Handler, irq9Handler, irq10Handler, irq11Handler,
      irq12Handler, irq13Handler, irq14Handler, irq15Handler, irq16Handler, irq17Handler,
      irq18Handler, irq19Handler, irq20Handler, irq21Handler, irq22Handler, irq23Handler
};

DISC_DATA multiboot_info_t *multibootInfo;

extern int initMemory(multiboot_info_t *info);
extern int readAcpiTables(void);
extern int loadServers(multiboot_info_t *info);

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

void stopInit(const char *msg) {
  disableInt();
  kprintf("Init failed: %s\nSystem halted.", msg);

  while(1)
    __asm__("hlt");
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
    addIDTEntry(CPU_EX_ISRS[i], i, 0);

  for(unsigned int i = 0; i < NUM_IRQS; i++)
    addIDTEntry(IRQ_ISRS[i], IRQ(i), 0);

  for(int i = 0; i < 16; i++)
    disableIRQ(i);

  //initPIC();
  loadIDT();
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
  kprintf("Mulitboot Information Flags:\n\n");

  for(size_t i = 0; i < 12; i++) {
    if(IS_FLAG_SET(info->flags, (1u << i)))
      kprintf("%s\n", MB_INFO_NAMES[i]);
  }
  kprintf("\n");
}

void showCPU_Features(void) {
  union ProcessorVersion procVersion;
  union AdditionalCpuidInfo additionalInfo;
  uint32_t features2;
  uint32_t features;

  __cpuid(1, procVersion.value, additionalInfo.value, features2, features);

  kprintf("CPUID Features:\n");

  kprintf("%s\n", PROCESSOR_TYPE[procVersion.processorType]);

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
      kprintf(" %s", FEATURES_STR[i]);
  }

  for(size_t i = 0; i < 32; i++) {
    if(IS_FLAG_SET(features2, 1 << i))
      kprintf(" %s", FEATURES_STR2[i]);
  }

  kprintf("\n");
}
#endif /* DEBUG */

/**
 Bootstraps the kernel.

 @param info The multiboot structure passed by the bootloader.
 */

void init(multiboot_info_t *info) {
  /* Initialize memory */

  if(IS_ERROR(initMemory(info)))
    stopInit("Unable to initialize memory.");

  info = (multiboot_info_t*)KPHYS_TO_VIRT(info);
  multibootInfo = info;

#ifdef DEBUG
  init_serial();
  initVideo();
  clearScreen();
#endif /* DEBUG */

  showMBInfoFlags(info);
  showCPU_Features();

  kprintf("Initializing interrupt handling.\n");
  initInterrupts();

  int retval = readAcpiTables();

  if(IS_ERROR(retval))
    stopInit("Unable to read ACPI tables.");
  else {
    numProcessors = (size_t)retval;

    if(numProcessors == 0)
      numProcessors = 1;
  }

  //  enable_apic();
  //  init_apic_timer();
  /*
   kprintf("Initializing timer.\n");
   initTimer();
   */

  kprintf("\n%#x bytes of discardable code.",
          (addr_t)EXT_PTR(kdData) - (addr_t)EXT_PTR(kdCode));
  kprintf(" %#x bytes of discardable data.\n",
          (addr_t)EXT_PTR(kdEnd) - (addr_t)EXT_PTR(kdData));
  kprintf("Discarding %d bytes in total\n",
          (addr_t)EXT_PTR(kdEnd) - (addr_t)EXT_PTR(kdCode));

  loadServers(info);

  /* Release the pages for the code and data that will never be used again. */

  for(addr_t addr = (addr_t)EXT_PTR(kdCode); addr < (addr_t)EXT_PTR(kBss);
      addr += PAGE_SIZE)
  {
    // todo: Mark these pages as free for init server
  }

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
