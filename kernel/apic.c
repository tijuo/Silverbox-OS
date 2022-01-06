#include <kernel/apic.h>
#include <kernel/mm.h>
#include <kernel/debug.h>
#include <kernel/lowlevel.h>
#include <kernel/io.h>
#include <x86intrin.h>

extern pmap_entry_t pml4_table;
extern uint32_t ap_bootstrap_start;
extern volatile uint16_t *ap_boot_count;

void apic_init(void);
void apic_init_aps(void);

void *lapic_ptr;
void *ioapic_ptr;

bool lapic_mapped;

/* APIC memory must be mapped as strong uncachable(UC)
 - page 428 of IA32_SDM_Vol3A */

void apic_send_eoi(void) {
  apic_reg_t *eoi_reg = LAPIC_REG(LAPIC_EOI);

  *eoi_reg = 0;
}

void apic_init(void) {
  paging_map_page(LAPIC_VADDR, (paddr_t)lapic_ptr, PAE_RW | PAE_PCD | PAE_PWT | PAE_XD, (paddr_t)&pml4_table);

  lapic_mapped = true;
  uint32_t apic_result = (uint32_t)rdmsr(IA32_APIC_BASE_MSR);

  kprintf("%s processor\n", IS_FLAG_SET(apic_result, LAPIC_BP) ? "Base" : "Application");
  kprintf("APIC %sabled\n", IS_FLAG_SET(apic_result, LAPIC_ENABLE) ? "en" : "dis");
  kprintf("APIC Base: 0x%p\n", (void *)((uintptr_t) (apic_result & ~(PAGE_SIZE - 1))));

  apic_reg_t *spurious_vector = LAPIC_REG(LAPIC_SPURIOUS_INT);

  // Set spurious vector to 255 and enable APIC

  *spurious_vector |= LAPIC_SPURIOUS_ENABLE | 255u;

  // Init timer

  apic_reg_t *timer_reg = LAPIC_REG(LAPIC_LVT_TIMER);
  apic_reg_t *div_conf_reg = LAPIC_REG(LAPIC_TIMER_DCR);
  apic_reg_t *init_counter = LAPIC_REG(LAPIC_INIT_COUNT);

  // Set timer to periodic mode, unmask the interrupt, and set interrupt vector to 32

  SET_FLAG(*timer_reg, LAPIC_PERIODIC | LAPIC_UNMASKED | 0x20);

  *div_conf_reg = LAPIC_TIMER_DIV_128; // Divide counter by 128
  *init_counter = 0x100000;

  apic_init_aps();
}

uint32_t apic_read_esr(void) {
  return (uint32_t)(*LAPIC_REG(LAPIC_ESR));
}

void *apic_get_base_addr(void) {
  return (void *)(uintptr_t)(rdmsr(IA32_APIC_BASE_MSR) & max_phys_addr & ~(PAGE_SIZE - 1));
}

bool apic_is_base_proc(void) {
  return (void *)(uintptr_t)(rdmsr(IA32_APIC_BASE_MSR) & max_phys_addr & ~(PAGE_SIZE - 1));
}

void apic_init_aps(void) {
  unsigned long int t1;
  apic_reg_t *icr = (apic_reg_t *)LAPIC_REG(LAPIC_ICR0);

  // Send INIT to all APs

  *icr = LAPIC_DEST_ALL_EX | LAPIC_TRIG_EDGE | LAPIC_LEVEL_ASSERT | LAPIC_DEST_PHYSICAL | LAPIC_DELIVERY_INIT | 0x00u;

  // Wait 100 milliseconds (use timestamp counter and assume 2 GHz speed)

  t1 = (unsigned long int)__rdtsc();

  while(__rdtsc() - t1 < 100000000)
    __pause();

  // Send two SIPIs to APs

  int stop_loop=0;

  for(int i=0; i < 2 ; i++) {
    *icr = LAPIC_DEST_ALL_EX | LAPIC_TRIG_EDGE | LAPIC_LEVEL_ASSERT | LAPIC_DEST_PHYSICAL | LAPIC_DELIVERY_START
           | (apic_reg_t)(ap_bootstrap_start >> PAGE_BITS);

    // Wait 200 microeconds

    t1 = (unsigned long int)__rdtsc();

    while(__rdtsc() - t1 < 400000) {
      if(*ap_boot_count == num_processors-1) {
        stop_loop = 1;
        break;
      }

      __pause();
    }

    if(stop_loop)
      break;
  }

  kprintf("%hhu APs initialized.\n", *ap_boot_count);
}
