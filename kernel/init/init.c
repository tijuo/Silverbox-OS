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

DISC_CODE static void disable_irq(unsigned int irq);

DISC_DATA ALIGNED(PAGE_SIZE) uint8_t kboot_stack[4 * PAGE_SIZE];
DISC_DATA void *kboot_stack_top = kboot_stack + sizeof kboot_stack;

extern idt_entry_t kernel_idt[NUM_EXCEPTIONS + NUM_IRQS];

extern uint8_t *kernel_stack_top;

DISC_CODE static void init_interrupts(void);
//DISC_CODE static void initTimer( void ));

DISC_CODE static void stop_init(const char*);
DISC_CODE void init(multiboot_info_t*);
//DISC_CODE static void initPIC( void ));

DISC_CODE void add_idt_entry(void (*f)(void), unsigned int entry_num,
                           unsigned int dpl);
DISC_CODE void load_idt(void);

DISC_DATA static void (*CPU_EX_ISRS[NUM_EXCEPTIONS])(
    void) = {
      cpu_ex0_handler, cpu_ex1_handler, cpu_ex2_handler, cpu_ex3_handler, cpu_ex4_handler, cpu_ex5_handler,
      cpu_ex6_handler, cpu_ex7_handler, cpu_ex8_handler, cpu_ex9_handler, cpu_ex10_handler, cpu_ex11_handler,
      cpu_ex12_handler, cpu_ex13_handler, cpu_ex14_handler, cpu_ex15_handler, cpu_ex16_handler, cpu_ex17_handler,
      cpu_ex18_handler, cpu_ex19_handler, cpu_ex20_handler, cpu_ex21_handler, cpu_ex22_handler, cpu_ex23_handler,
      cpu_ex24_handler, cpu_ex25_handler, cpu_ex26_handler, cpu_ex27_handler, cpu_ex28_handler, cpu_ex29_handler,
      cpu_ex30_handler, cpu_ex31_handler
};

DISC_DATA static void (*IRQ_ISRS[NUM_IRQS])(
    void) = {
      irq0_handler, irq1_handler, irq2_handler, irq3_handler, irq4_handler, irq5_handler,
      irq6_handler, irq7_handler, irq8_handler, irq9_handler, irq10_handler, irq11_handler,
      irq12_handler, irq13_handler, irq14_handler, irq15_handler, irq16_handler, irq17_handler,
      irq18_handler, irq19_handler, irq20_handler, irq21_handler, irq22_handler, irq23_handler
};

DISC_DATA multiboot_info_t *multiboot_info;

extern int init_memory(multiboot_info_t *info);
extern int read_acpi_tables(void);
extern int load_servers(multiboot_info_t *info);

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

void add_idt_entry(void (*f)(void), unsigned int entry_num, unsigned int dpl) {
  assert(entry_num < 256);

  idt_entry_t *new_entry = &kernel_idt[entry_num];

  new_entry->offset_lower = (uint16_t)((uint32_t)f & 0xFFFFu);
  new_entry->code_selector = KCODE_SEL;
  new_entry->_resd = 0;
  new_entry->flags = I_PRESENT | (MIN(dpl, 3) << I_DPL_BITS) | I_INT;
  new_entry->offset_upper = (uint16_t)((uint32_t)f >> 16);
}

void load_idt(void) {
  struct PseudoDescriptor idt_pointer = {
    .limit = KERNEL_IDT_LEN,
    .base = (uint64_t)kernel_idt
  };

  __asm__("lidt %0" :: "m"(idt_pointer) : "memory");
}

void disable_irq(unsigned int irq) {
  assert(irq < 16);

  // Send OCW1 (set IRQ mask)

  if(irq < PIC2_IRQ_START)
    out_port8( PIC1_PORT | 0x01,
             in_port8(PIC1_PORT | 0x01) | (uint8_t)FROM_FLAG_BIT(irq));
  else
    out_port8(
        PIC2_PORT | 0x01,
        in_port8(PIC2_PORT | 0x01) | (uint8_t)FROM_FLAG_BIT(irq-PIC2_IRQ_START));
}

void stop_init(const char *msg) {
  disable_int();
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

void init_interrupts(void) {
  for(unsigned int i = 0; i < NUM_EXCEPTIONS; i++)
    add_idt_entry(CPU_EX_ISRS[i], i, 0);

  for(unsigned int i = 0; i < NUM_IRQS; i++)
    add_idt_entry(IRQ_ISRS[i], IRQ(i), 0);

  for(int i = 0; i < 16; i++)
    disable_irq(i);

  //initPIC();
  load_idt();
}

#ifdef DEBUG
DISC_CODE static void show_cpu_features(void);
DISC_CODE static void show_mb_info_flags(multiboot_info_t*);

union ProcessorVersion {
  struct {
    uint32_t stepping_id :4;
    uint32_t model :4;
    uint32_t family_id :4;
    uint32_t processor_type :2;
    uint32_t _resd :2;
    uint32_t ext_model_id :4;
    uint32_t ext_family_id :8;
    uint32_t _resd2 :4;
  };
  uint32_t value;
};

union AdditionalCpuidInfo {
  struct {
    uint8_t brand_index;
    uint8_t cflush_size;
    uint8_t max_logical_processors;
    uint8_t lapic_id;
  };
  uint32_t value;
};

void show_mb_info_flags(multiboot_info_t *info) {
  kprintf("Mulitboot Information Flags:\n\n");

  for(size_t i = 0; i < 12; i++) {
    if(IS_FLAG_SET(info->flags, (1u << i)))
      kprintf("%s\n", MB_INFO_NAMES[i]);
  }
  kprintf("\n");
}

void show_cpu_features(void) {
  union ProcessorVersion proc_version;
  union AdditionalCpuidInfo additional_info;
  uint32_t features2;
  uint32_t features;

  __cpuid(1, proc_version.value, additional_info.value, features2, features);

  kprintf("CPUID Features:\n");

  kprintf("%s\n", PROCESSOR_TYPE[proc_version.processor_type]);

  kprintf(
      "Model: %d\n",
      proc_version.model + (
          (proc_version.family_id == 6 || proc_version.family_id == 15) ?
              (proc_version.ext_model_id << 4) : 0));

  kprintf(
      "Family: %d\n",
      proc_version.family_id + (
          proc_version.family_id == 15 ? proc_version.ext_family_id : 0));
  kprintf("Stepping: %d\n\n", proc_version.stepping_id);

  kprintf("Brand Index: %d\n", additional_info.brand_index);

  if(IS_FLAG_SET(features, 1 << 19))
    kprintf("Cache Line Size: %d bytes\n", 8 * additional_info.cflush_size);

  if(IS_FLAG_SET(features, 1 << 28)) {
    size_t num_ids;

    if(additional_info.max_logical_processors) {
      uint32_t msb = _bit_scan_reverse(additional_info.max_logical_processors);

      if(additional_info.max_logical_processors & ((1 << msb) - 1))
        num_ids = (1 << (msb + 1));
      else
        num_ids = 1 << msb;
    }
    else
      num_ids = 0;

    kprintf("Unique APIC IDs per physical processor: %d\n", num_ids);
  }
  kprintf("Local APIC ID: %#x\n\n", additional_info.lapic_id);

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

  if(IS_ERROR(init_memory(info)))
    stop_init("Unable to initialize memory.");

  multiboot_info = info;

#ifdef DEBUG
  init_serial();
  init_video();
  clear_screen();
#endif /* DEBUG */

  show_mb_info_flags(info);
  show_cpu_features();

  kprintf("Initializing interrupt handling.\n");
  init_interrupts();

  int retval = read_acpi_tables();

  if(IS_ERROR(retval))
    stop_init("Unable to read ACPI tables.");
  else {
    num_processors = (size_t)retval;

    if(num_processors == 0)
      num_processors = 1;
  }

  //  enable_apic();
  //  init_apic_timer();
  /*
   kprintf("Initializing timer.\n");
   initTimer();
   */

  kprintf("\n%#x bytes of discardable code.",
          (addr_t)EXT_PTR(kddata) - (addr_t)EXT_PTR(kdcode));
  kprintf(" %#x bytes of discardable data.\n",
          (addr_t)EXT_PTR(kdend) - (addr_t)EXT_PTR(kddata));
  kprintf("Discarding %d bytes in total\n",
          (addr_t)EXT_PTR(kdend) - (addr_t)EXT_PTR(kdcode));

  load_servers(info);

  /* Release the pages for the code and data that will never be used again. */

  for(addr_t addr = (addr_t)EXT_PTR(kdcode); addr < (addr_t)EXT_PTR(kbss);
      addr += PAGE_SIZE)
  {
    // todo: Mark these pages as free for init server
  }

  // Initialize FPU to a known state
  __asm__("fninit\n"
		  "fxsave %0\n" :: "m"(init_server_thread->xsave_state));

  // Set MSRs to enable sysenter/sysexit functionality

  wrmsr(SYSENTER_CS_MSR, KCODE_SEL);
  wrmsr(SYSENTER_ESP_MSR, (uint64_t)(uintptr_t)kernel_stack_top);
  wrmsr(SYSENTER_EIP_MSR, (uint64_t)(uintptr_t)sysenter_entry);

  kprintf("Context switching...\n");

  switch_context(schedule(0), 0);
  stop_init("Error: Context switch failed.");
}
