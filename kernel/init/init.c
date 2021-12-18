#include <elf.h>

#include <kernel/interrupt.h>
#include <kernel/thread.h>
#include <kernel/debug.h>
#include <kernel/mm.h>
#include <kernel/schedule.h>
#include <kernel/pae.h>
#include <kernel/memory.h>
#include <kernel/mm.h>
#include <kernel/error.h>
#include <bits.h>
#include <kernel/syscall.h>
#include <kernel/lowlevel.h>
#include <cpuid.h>
#include <x86intrin.h>
#include <limits.h>
#include <kernel/apic.h>
#include <kernel/pic.h>
#include <os/io.h>
#include "init.h"
#include <kernel/multiboot2.h>

#define KERNEL_IDT_LEN	(64 * sizeof(union idt_entry))

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
  "sse3",
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

DISC_CODE static void disable_pics(void);

DISC_DATA ALIGNED(PAGE_SIZE) uint8_t kboot_stack[4 * PAGE_SIZE];
DISC_DATA void *kboot_stack_top = kboot_stack + sizeof kboot_stack;

extern idt_entry_t kernel_idt[NUM_EXCEPTIONS + NUM_IRQS];

DISC_CODE static void init_interrupts(void);
//DISC_CODE static void initTimer( void ));

DISC_CODE static void stop_init(const char*);
DISC_CODE void init(const struct multiboot_info_header*);
//DISC_CODE static void initPIC( void ));

DISC_CODE void add_idt_entry(void (*f)(void), int entry_num, int dpl);
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

extern int init_memory(const struct multiboot_info_header *tags);
extern int read_acpi_tables(void);

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

void add_idt_entry(void (*f)(void), int entry_num, int dpl) {
  assert(entry_num < 256);

  idt_entry_t *new_entry = &kernel_idt[entry_num];

  new_entry->value = 0;
  new_entry->offset_lower = (uint16_t)((uintptr_t)f & 0xFFFFu);
  new_entry->code_selector = KCODE_SEL;
  new_entry->flags = I_PRESENT | (MIN(dpl, 3) << I_DPL_BITS) | I_INT;
  new_entry->offset_mid = (uint16_t)((uintptr_t)f >> 16);
  new_entry->offset_upper = (uint32_t)((uintptr_t)f >> 32);
}

void load_idt(void) {
  struct pseudo_descriptor idt_pointer = {
	.limit = KERNEL_IDT_LEN,
	.base = (uint64_t)kernel_idt
  };

  __asm__("lidt %0" :: "m"(idt_pointer) : "memory");
}

void disable_pics(void) {
  // Send OCW1 (set IRQ mask)

  out_port8(PIC1_PORT | 0x01, in_port8(PIC1_PORT | 0x01) | 0xFF);
  out_port8(PIC2_PORT | 0x01, in_port8(PIC2_PORT | 0x01) | 0xFF);
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
  for(size_t i = 0; i < NUM_EXCEPTIONS; i++)
	add_idt_entry(CPU_EX_ISRS[i], (int)i, 0);

  for(size_t i = 0; i < NUM_IRQS; i++)
	add_idt_entry(IRQ_ISRS[i], (int)IRQ(i), 0);

  disable_pics();

  //initPIC();
  load_idt();
}

#ifdef DEBUG
DISC_CODE static void show_cpu_features(void);
DISC_CODE static void show_mb_info(const struct multiboot_info_header*);

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

void show_mb_info(const struct multiboot_info_header *header) {
  kprintf("Mulitboot2 Information:\n\n");

  const struct multiboot_tag *tags = header->tags;

  while(tags->type != MULTIBOOT_TAG_TYPE_END) {
	switch(tags->type) {
	  case MULTIBOOT_TAG_TYPE_BASIC_MEMINFO: {
		const struct multiboot_tag_basic_meminfo *meminfo =
		  (const struct multiboot_tag_basic_meminfo*)tags;

		unsigned long int total_phys_mem = (meminfo->mem_upper + 1024) * 1024ul;

		kprintf("Lower Memory: %lu bytes Upper Memory: %lu bytes\n",
				meminfo->mem_lower * 1024ul, meminfo->mem_upper * 1024ul);
		kprintf("Total Memory: %#x. %lu pages.\n", total_phys_mem,
				total_phys_mem / PAGE_SIZE);

		break;
	  }
	  case MULTIBOOT_TAG_TYPE_BOOTDEV: {
		const struct multiboot_tag_bootdev *bootdev =
		  (const struct multiboot_tag_bootdev*)tags;

		kprintf("Boot drive: ");

		if(bootdev->biosdev != 0xFFFFFFFFu)
		  kprintf("%s%u", bootdev->biosdev & 0x80 ? "hd" : "fd",
				  bootdev->biosdev & 0x7F);

		if(bootdev->slice != 0xFFFFFFFFu)
		  kprintf("p%u", bootdev->slice);

		if(bootdev->part != 0xFFFFFFFFu)
		  kprintf(".%u", bootdev->part);

		kprintf("\n");

		break;
	  }
	  case MULTIBOOT_TAG_TYPE_CMDLINE: {
		const struct multiboot_tag_string *cmdline =
		  (const struct multiboot_tag_string*)tags;

		kprintf("Command: \"%s\"\n", cmdline->string);
		break;
	  }
	  case MULTIBOOT_TAG_TYPE_MMAP: {
		const struct multiboot_tag_mmap *mmap = (const struct multiboot_tag_mmap*)tags;
		const struct multiboot_mmap_entry *entry = mmap->entries;
		size_t offset = offsetof(const struct multiboot_tag_mmap, entries);
		const char *type;

		kprintf("Memory map: \n");

		while(offset < mmap->size) {
		  switch(entry->type) {
			case MULTIBOOT_MEMORY_AVAILABLE:
			  type = "Available";
			  break;
			case MULTIBOOT_MEMORY_ACPI_RECLAIMABLE:
			  type = "ACPI Reclaimable";
			  break;
			case MULTIBOOT_MEMORY_NVS:
			  type = "Non-Volatile Storage";
			  break;
			case MULTIBOOT_MEMORY_BADRAM:
			  type = "Defective RAM";
			  break;
			case MULTIBOOT_MEMORY_RESERVED:
			default:
			  type = "Reserved";
			  break;
		  }

		  kprintf("Addr: %p Length: %#x Type: %s\n", (void*)entry->addr,
				  entry->len, type);

		  offset += mmap->entry_size;
		  entry = (const struct multiboot_mmap_entry*)((uintptr_t)entry
			  + mmap->entry_size);
		}

		break;
	  }
	  case MULTIBOOT_TAG_TYPE_MODULE: {
		const struct multiboot_tag_module *module = (const struct multiboot_tag_module*)tags;

		kprintf("Module: load addr: %p size: %u command: \"%s\"\n",
				(void *)(uintptr_t)module->mod_start, module->mod_end - module->mod_start,
				module->cmdline);

		break;
	  }
	  case MULTIBOOT_TAG_TYPE_EFI32: {
		const struct multiboot_tag_efi32 *efi = (const struct multiboot_tag_efi32*)tags;

		kprintf("EFI 32-bit system table: %p\n", (void*)(uintptr_t)efi->pointer);
		break;
	  }
	  case MULTIBOOT_TAG_TYPE_EFI64: {
		const struct multiboot_tag_efi64 *efi = (const struct multiboot_tag_efi64*)tags;

		kprintf("EFI 64-bit system table: %p\n", (void*)efi->pointer);
		break;
	  }
	  default:
		kprintf("Multiboot Info Tag: %u\n", tags->type);
		break;
	  case MULTIBOOT_TAG_TYPE_END:
		return;
	}

	tags = (const struct multiboot_tag*)((uintptr_t)tags + tags->size + ((tags->size % 8) == 0 ? 0 : 8 - (tags->size % 8)));
  }
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

void init(const struct multiboot_info_header *info_header) {
  /* Initialize memory */

  if(IS_ERROR(init_memory(info_header)))
	stop_init("Unable to initialize memory.");

#ifdef DEBUG
  init_serial();
  init_video();
  clear_screen();
#endif /* DEBUG */

  show_mb_info(info_header);
  show_cpu_features();

  kprintf("1 GiB pages: %s\n", is_1gb_pages_supported ? "yes" : "no");
  kprintf("Maximum physical address: %#lx\n", max_phys_addr);

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

  //load_init_server(info_header);

  /* Release the pages for the code and data that will never be used again. */

  for(addr_t addr = (addr_t)EXT_PTR(kdcode); addr < (addr_t)EXT_PTR(kbss);
	  addr += PAGE_SIZE)
  {
	// todo: Mark these pages as free for init server
  }

  // Initialize FPU to a known state
  __asm__("fninit\n");

  // Set MSRs to enable syscall/sysret functionality

  wrmsr(SYSCALL_FMASK_MSR, RFLAGS_IF | RFLAGS_DF);
  wrmsr(SYSCALL_STAR_MSR,
		((uint64_t)KCODE_SEL << 32) | ((uint64_t)UCODE_SEL << 48));
  wrmsr(SYSCALL_LSTAR_MSR, (uint64_t)syscall_entry);

  kprintf("Context switching...\n");

  switch_context(schedule(get_current_processor()));
  stop_init("Context switch failed.");
}
