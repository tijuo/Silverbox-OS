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
#include <kernel/lowlevel.h>
#include <cpuid.h>
#include <limits.h>
#include <kernel/apic.h>
#include <kernel/pic.h>
#include <os/io.h>
#include "init.h"
#include "pit.h"
#include <stdatomic.h>
#include <x86gprintrin.h>

#define KERNEL_IDT_LEN	(64 * sizeof(struct IdtEntry))

extern noreturn void sysenter_entry(void) NAKED;

typedef struct {
    uint32_t num;
    uint32_t denom;
} Rational;

SECTION(".header") multiboot_header_t mboot = {
    .magic = MULTIBOOT_HEADER_MAGIC,
    .flags = MULTIBOOT_PAGE_ALIGN | MULTIBOOT_MEMORY_INFO | MULTIBOOT_VIDEO_TABLE,
    .checksum = -(MULTIBOOT_HEADER_MAGIC + (MULTIBOOT_PAGE_ALIGN | MULTIBOOT_MEMORY_INFO | MULTIBOOT_VIDEO_TABLE)),
    .mode_type = MULTIBOOT_EGA_TEXT,
    .width = MULTIBOOT_EGA_WIDTH,
    .height = MULTIBOOT_EGA_HEIGHT,
    .depth = MULTIBOOT_EGA_DEPTH
};

DISC_DATA const char* MB_INFO_NAMES[] = {
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

DISC_DATA const char* FEATURES_STR[32] = {
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

DISC_DATA const char* FEATURES_STR2[32] = {
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

DISC_DATA const char* PROCESSOR_TYPE[4] = {
  "OEM Processor",
  "Intel Overdrive Processor",
  "Dual Processor",
  "???"
};

DISC_DATA Rational pit_freq_hz = {
    .num = 3579545,
    .denom = 3
};

DISC_CODE static void enable_irq(unsigned int irq);
DISC_CODE static void disable_irq(unsigned int irq);

DISC_DATA ALIGN_AS(PAGE_SIZE) uint8_t kboot_stack[4 * PAGE_SIZE];
DISC_DATA void* kboot_stack_top = kboot_stack + sizeof kboot_stack;

DISC_DATA bool apic_calibrated = false;
DISC_DATA unsigned int apic_ticks = 0;
DISC_CODE NAKED noreturn void pit_handler(void);
DISC_CODE uint16_t get_pit_count(void);

extern idt_entry_t kernel_idt[NUM_EXCEPTIONS + NUM_IRQS];
extern uint8_t* kernel_stack_top;

DISC_CODE static void init_interrupts(void);
//DISC_CODE static void initTimer( void ));

DISC_CODE static void stop_init(const char*);
DISC_CODE void init(multiboot_info_t*);

#ifdef ENABLE_PIC
DISC_CODE static void init_pic(void);
#endif /* ENABLE_PIC */

DISC_CODE void add_idt_entry(void (*f)(void), unsigned int entry_num,
    unsigned int dpl);
DISC_CODE void load_idt(void);
DISC_CODE void init_apic_timer(void);
DISC_CODE int enable_apic(void);
DISC_CODE int setup_pit_sleep(unsigned int microseconds);

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

DISC_DATA multiboot_info_t* multiboot_info;

extern int init_memory(multiboot_info_t* info);
extern int read_acpi_tables(void);
extern int load_servers(multiboot_info_t* info);
extern paddr_t alloc_phys_frame(void);

#ifdef ENABLE_PIC
void init_pic(void)
{
    // Send ICW1 (cascade, edge-triggered, ICW4 needed)
    out_port8((uint16_t)0x20, 0x11);
    out_port8((uint16_t)0xA0, 0x11);
    io_wait();

    // Send ICW2 (Set interrupt vector)
    out_port8((uint16_t)0x21, IRQ(0));
    out_port8((uint16_t)0xA1, IRQ(8));
    io_wait();

    // Send ICW3 (IRQ2 input has a slave)
    out_port8((uint16_t)0x21, 0x04);

    // Send ICW3 (Slave id 0x02)
    out_port8((uint16_t)0xA1, 0x02);
    io_wait();

    // Send ICW4 (Intel 8086 mode)
    out_port8((uint16_t)0x21, 0x01);
    out_port8((uint16_t)0xA1, 0x01);
    io_wait();

    // Send OCW1 (Set mask to 0xFF)

    out_port8((uint16_t)0x21, 0xFF);
    out_port8((uint16_t)0xA1, 0xFF);
    io_wait();


    //kprintfln("Enabling IRQ 2");
    //enableIRQ( 2 );

    // disable all IRQs
    out_port8(PIC1_PORT | 0x01, 0xFF);
    out_port8(PIC2_PORT | 0x01, 0xFF);
    io_wait();
}
#endif /* ENABLE_PIC */

/* Various call once functions */

void init_apic_timer(void)
{
    apic_ptr_t timer_reg = LAPIC_REG(LAPIC_TIMER);
    apic_ptr_t timer_div_reg = (apic_ptr_t)LAPIC_REG(LAPIC_TIMER_DCR);
    apic_ptr_t init_count_reg = (apic_ptr_t)LAPIC_REG(LAPIC_TIMER_IC);
    apic_ptr_t current_count_reg = (apic_ptr_t)LAPIC_REG(LAPIC_TIMER_CC);

    *timer_reg = LAPIC_ONESHOT | LAPIC_MASKED | IRQ(0);
    *timer_div_reg = 0b0000; // Divide by 2

    #define NUM_ITERS   10
    unsigned int counts[NUM_ITERS];

    for(int i=0; i < NUM_ITERS; i++) {
        counts[i] = 0xFFFFFFFFu;
    }

    int a, b, c, d;

    for(int i=0; i < NUM_ITERS; i++) {
        // Don't reorder these stores
        __cpuid(0, a, b, c, d);

        setup_pit_sleep(10000);

        __cpuid(0, a, b, c, d);

        *init_count_reg = 0xFFFFFFFFu;

        __cpuid(0, a, b, c, d);


        //enable_irq(0);
        //enable_int();

        out_port8((uint16_t)TIMER0, (uint8_t)(C_SELECT0 | C_MODE0 | BIN_COUNTER | RWL_FORMAT0));

        // Wait for the pit count to equal 0

        uint16_t low, hi;

        while(true) {
            low = in_port8((uint16_t)TIMER0);
            hi = in_port8((uint16_t)TIMER0);

            if(!low && !hi) break;
        }

        __cpuid(0, a, b, c, d);

        counts[i] -= *current_count_reg;
    }

    apic_ticks = 0;

    for(int i=0; i < NUM_ITERS; i++) {
        apic_ticks += counts[i];
    }

    apic_ticks /= NUM_ITERS;
    apic_calibrated = true;

    kprintfln("Avg. number of ticks in 10 ms: %u", apic_ticks);
}

int enable_apic(void)
{
    uint32_t apic_result = (uint32_t)rdmsr(IA32_APIC_BASE_MSR);

    if((apic_result & (1u << 8)) != 0) {
        kprintfln("Base processor");
    }

    if((apic_result & (1u << 11)) != 0) {
        kprintfln("APIC enabled");
    } else {
        kprintfln("APIC disabled");
    }

    kprintfln("APIC Base: %#p", (void*)(apic_result & PAGE_BASE_MASK));

    // Map the LAPIC and IOAPIC

    // This assumes that the PDE for the LAPIC ond IOAPICs has already been mapped

    /*
    pde_t *apic_pde = CURRENT_PDE(LAPIC_VADDR);
    paddr_t ptab_base;

    if(!apic_pde.is_present) {
        ptab_base = alloc_phys_frame();

        if(IS_ERROR(clear_phys_page(ptab_base))) {
            kprintfln("Error: Unable to clear entries in page table.");
        }

        apic_pde.value = 0;

        apic_pde.base = PADDR_TO_PDE_BASE(ptab_base);
        apic_pde.is_read_write = 1;
        apic_pde.available2 = 1; // Indicate that the base address of the page table was allocated at boot time.
        apic_pde.is_present = 1;

        if(IS_ERROR(write_pde(PDE_INDEX(LAPIC_VADDR), apic_pde, CURRENT_ROOT_PMAP))) {
            RET_MSG(E_FAIL, "Unable to create PDE for Local APIC.");
        }
    } else {
        ptab_base = PDE_BASE(apic_pde);
    }
*/

    pte_t *pte = CURRENT_PTE(LAPIC_VADDR);

    pte->value = 0;
    pte->is_read_write = 1;
    pte->global = 1;
    pte->pat = 0;
    pte->pcd = 1;
    pte->pwt = 1;
    pte->base = PADDR_TO_PTE_BASE((paddr_t)lapic_ptr);
    pte->is_present = 1;
    invalidate_page(LAPIC_VADDR);

    // This assumes there's only one IOAPIC in the system

    pte = CURRENT_PTE(IOAPIC_VADDR);

    pte->value = 0;
    pte->is_read_write = 1;
    pte->global = 1;
    pte->pcd = 1;
    pte->pwt = 1;
    pte->base = PADDR_TO_PTE_BASE((paddr_t)ioapic_ptr);
    pte->is_present = 1;

    invalidate_page(IOAPIC_VADDR);

    // TODO: Setup the SVT

    return 0;
}

void add_idt_entry(void (*f)(void), unsigned int entry_num, unsigned int dpl)
{
    KASSERT(entry_num < 256);

    idt_entry_t* new_entry = &kernel_idt[entry_num];

    new_entry->offset_lower = (uint16_t)((uint32_t)f & 0xFFFFu);
    new_entry->selector = KCODE_SEL;
    new_entry->_resd = 0;
    new_entry->gate_type = I_INT;
    new_entry->is_storage = 0;
    new_entry->dpl = MIN(dpl, 3u);
    new_entry->is_present = 1;
    new_entry->offset_upper = (uint16_t)((uint32_t)f >> 16);
}

void load_idt(void)
{
    struct IdtPointer idt_pointer = {
      .limit = KERNEL_IDT_LEN,
      .base = (uint32_t)kernel_idt
    };

    __asm__("lidt %0" :: "m"(idt_pointer) : "memory");
}

void enable_irq(unsigned int irq)
{
    KASSERT(irq < 16);

    if(irq < PIC2_IRQ_START) {
        out_port8(PIC1_PORT | 0x01, in_port8(PIC1_PORT | 0x01) & ~(uint8_t)FROM_FLAG_BIT(irq));
    } else {
        out_port8(PIC2_PORT | 0x01, in_port8(PIC2_PORT | 0x01) & ~(uint8_t)FROM_FLAG_BIT(irq - PIC2_IRQ_START));
    }
}

void disable_irq(unsigned int irq)
{
    KASSERT(irq < 16);

    // Send OCW1 (set IRQ mask)

    if(irq < PIC2_IRQ_START) {
        out_port8(PIC1_PORT | 0x01, in_port8(PIC1_PORT | 0x01) | (uint8_t)FROM_FLAG_BIT(irq));
    } else {
        out_port8(PIC2_PORT | 0x01, in_port8(PIC2_PORT | 0x01) | (uint8_t)FROM_FLAG_BIT(irq - PIC2_IRQ_START));
    }
}

void stop_init(const char* msg)
{
    disable_int();
    kprintfln("Init failed: %s\nSystem halted.", msg);

    while(1)
        __asm__("hlt");
}

// Set PIT Counter
int setup_pit_sleep(unsigned int microseconds)
{
    if(microseconds > 50000u) {
        PANIC("Unable to setup PIT timer. Microseconds must not be greater than 50000");
        return -1;
    }

    uint64_t num = pit_freq_hz.num * (uint64_t)microseconds;
    uint64_t denom = pit_freq_hz.denom * 1000000;
    uint16_t count = (uint16_t)(num / denom);

    out_port8((uint16_t)TIMER_CTRL, (uint8_t)(C_SELECT0 | C_MODE0 | BIN_COUNTER | RWL_FORMAT3));
    out_port8((uint16_t)TIMER0, (uint8_t)(count & 0xFFu));
    out_port8((uint16_t)TIMER0, (uint8_t)(count >> 8));

    return 0;
}

uint16_t get_pit_count(void)
{
    uint16_t low, hi;

    low = in_port8((uint16_t)TIMER0);
    hi = in_port8((uint16_t)TIMER0);

    return low | (hi << 8u);
}

void init_interrupts(void)
{
    for(unsigned int i = 0; i < NUM_EXCEPTIONS; i++)
        add_idt_entry(CPU_EX_ISRS[i], i, 0);

    // Add pit handler

    for(unsigned int i = 0; i < NUM_IRQS; i++)
        add_idt_entry(IRQ_ISRS[i], IRQ(i), 0);

    for(int i = 0; i < 16; i++)
        disable_irq(i);

#ifdef ENABLE_PIC
    init_pic();
#endif /* ENABLE_PIC */

    load_idt();
}

#ifdef DEBUG
DISC_CODE static void show_cpu_features(void);
DISC_CODE static void show_mb_info_flags(multiboot_info_t*);

union ProcessorVersion {
    struct {
        uint32_t stepping_id : 4;
        uint32_t model : 4;
        uint32_t family_id : 4;
        uint32_t processor_type : 2;
        uint32_t _resd : 2;
        uint32_t ext_model_id : 4;
        uint32_t ext_family_id : 8;
        uint32_t _resd2 : 4;
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

void show_mb_info_flags(multiboot_info_t* info)
{
    kprintfln("Mulitboot Information Flags:");
    kprintfln("");

    for(size_t i = 0; i < 12; i++) {
        if(IS_FLAG_SET(info->flags, (1u << i)))
            kprintfln("%s", MB_INFO_NAMES[i]);
    }
    kprintfln("");
}

void show_cpu_features(void)
{
    union ProcessorVersion proc_version;
    union AdditionalCpuidInfo additional_info;
    uint32_t features2;
    uint32_t features;

    __cpuid(1, proc_version.value, additional_info.value, features2, features);

    kprintfln("CPUID Features:");

    kprintfln("%s", PROCESSOR_TYPE[proc_version.processor_type]);

    kprintfln(
        "Model: %d",
        proc_version.model + (
            (proc_version.family_id == 6 || proc_version.family_id == 15) ?
            (proc_version.ext_model_id << 4) : 0));

    kprintfln(
        "Family: %d",
        proc_version.family_id + (
            proc_version.family_id == 15 ? proc_version.ext_family_id : 0));
    kprintfln("Stepping: %d", proc_version.stepping_id);

    kprintfln("Brand Index: %d", additional_info.brand_index);

    if(IS_FLAG_SET(features, 1u << 19))
        kprintfln("Cache Line Size: %d bytes", 8 * additional_info.cflush_size);

    if(IS_FLAG_SET(features, 1u << 28)) {
        size_t num_ids;

        if(additional_info.max_logical_processors) {
            uint32_t msb = _bit_scan_reverse(additional_info.max_logical_processors);

            if(additional_info.max_logical_processors & ((1u << msb) - 1))
                num_ids = (1u << (msb + 1));
            else
                num_ids = 1u << msb;
        } else
            num_ids = 0;

        kprintfln("Unique APIC IDs per physical processor: %d", num_ids);
    }
    kprintfln("Local APIC ID: %#x", additional_info.lapic_id);

    kprintf("Processor Features:");

    for(size_t i = 0; i < 32; i++) {
        if(IS_FLAG_SET(features, 1u << i))
            kprintf(" %s", FEATURES_STR[i]);
    }

    for(size_t i = 0; i < 32; i++) {
        if(IS_FLAG_SET(features2, 1u << i))
            kprintf(" %s", FEATURES_STR2[i]);
    }

    kprintfln("");
}
#endif /* DEBUG */

/**
 Bootstraps the kernel.

 @param info The multiboot structure passed by the bootloader.
 */
void init(multiboot_info_t* info)
{
    /* Initialize memory */

    if(IS_ERROR(init_memory(info)))
        stop_init("Unable to initialize memory.");

    info = (multiboot_info_t*)PHYS_TO_VIRT((addr_t)info);
    multiboot_info = info;

#ifdef DEBUG
    init_serial();
    init_video();
    clear_screen();
#endif /* DEBUG */

    show_mb_info_flags(info);
    show_cpu_features();

    kprintfln("Initializing interrupt handling.");
    init_interrupts();

    int retval = read_acpi_tables();

    if(IS_ERROR(retval))
        stop_init("Unable to read ACPI tables.");
    else {
        num_processors = (size_t)retval;

        if(num_processors == 0)
            num_processors = 1;
    }

    kprintfln("Initializing APIC.");
    enable_apic();

    kprintfln("Initializing timer.");
    init_apic_timer();

    // Restory IRQ 0 handler
    //add_idt_entry(IRQ_ISRS[0], IRQ(0), 0);

    kprintfln("\n%#x bytes of discardable code. %#x bytes of discardable data.",
        (addr_t)EXT_PTR(kddata) - (addr_t)EXT_PTR(kdcode),
        (addr_t)EXT_PTR(kdend) - (addr_t)EXT_PTR(kddata));
    kprintfln("Discarding %d bytes in total",
        (addr_t)EXT_PTR(kdend) - (addr_t)EXT_PTR(kdcode));

    if(IS_ERROR(load_servers(info))) {
        stop_init("A module for the initial server indicated by the --initsrv flag must be loaded.");
    }

    /* Release the pages for the code and data that will never be used again. */

    for(addr_t addr = (addr_t)EXT_PTR(kdcode); addr < (addr_t)EXT_PTR(kbss);
        addr += PAGE_SIZE) {
        // todo: Mark these pages as free for init server
    }

// Set MSRs to enable sysenter/sysexit functionality

    wrmsr(SYSENTER_CS_MSR, KCODE_SEL);
    wrmsr(SYSENTER_ESP_MSR, (uint64_t)(uintptr_t)kernel_stack_top);
    wrmsr(SYSENTER_EIP_MSR, (uint64_t)(uintptr_t)sysenter_entry);

    // Set initial SSE state
    //_fxrstor(init_server_thread->fxsave_state);

    kprintfln("Context switching...");

    thread_switch_context(schedule(0), false);
    stop_init("Error: Context switch failed.");
}
