#include <kernel/apic.h>
#include <kernel/mm.h>
#include <kernel/debug.h>
#include <kernel/lowlevel.h>

void send_apic_eoi(void);
void init_apic_timer(void);
void enable_apic(void);

void *lapic_ptr;
void *ioapic_ptr;

typedef volatile unsigned long int * apic_ptr_t;

#define LAPIC_REG(x)  (apic_ptr_t)(LAPIC_VADDR + (x))

/* APIC memory must be mapped as strong uncachable(UC)
 - page 428 of IA32_SDM_Vol3A */

void send_apic_eoi(void) {
  apic_ptr_t eoi_reg = LAPIC_REG(LAPIC_EOI);

  *eoi_reg = 0;
}

void init_apic_timer(void) {
  apic_ptr_t timer_reg = LAPIC_REG(LAPIC_TIMER);
  apic_ptr_t div_conf_reg = LAPIC_REG(LAPIC_TIMER_DCR);

//  *timer_reg = LAPIC_PERIODIC | LAPIC_UNMASKED | 0x32;
//  *div_conf_reg = 0; // Divide counter by 2
}

void enable_apic(void) {
  uint32_t apic_result = (uint32_t)rdmsr(IA32_APIC_BASE_MSR);

  if((apic_result & (1u << 8)) != 0) {
    kprintf("Base processor\n");
  }

  if((apic_result & (1u << 11)) != 0) {
    kprintf("APIC enabled\n");
  }
  else {
    kprintf("APIC disabled\n");
  }

  kprintf("APIC Base: 0x%p\n", (void *)((uintptr_t) (apic_result & ~0xFFFu)));
/*
  // TODO: Here, actually *set up* the LAPIC
  kMapPage((addr_t)lapicPtr, (addr_t)LAPIC_BASE,
           PAGING_RW | PAGING_PCD | PAGING_PWT);
*/
  // TODO: Setup the SVT
}
