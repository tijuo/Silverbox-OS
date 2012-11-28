#include <kernel/apic.h>
#include <kernel/mm.h>
#include <kernel/debug.h>

void send_apic_eoi(void);
void init_apic_timer(void);
void enable_apic(void);

/* APIC memory must be mapped as strong uncachable(UC)
   - page 428 of IA32_SDM_Vol3A */

void send_apic_eoi(void)
{
  volatile unsigned *eoi_reg = (volatile unsigned *)(LAPIC_EOI + LAPIC_VADDR);

  *eoi_reg = 0;
}

void init_apic_timer(void)
{
  volatile unsigned *timer_reg = (volatile unsigned *)(LAPIC_TIMER + LAPIC_VADDR);
  volatile unsigned *div_conf_reg = (volatile unsigned *)(LAPIC_TIMER_DCR + LAPIC_VADDR);

//  *timer_reg = LAPIC_PERIODIC | LAPIC_UNMASKED | 0x32;
//  *div_conf_reg = 0; // Divide counter by 2
}

void enable_apic(void)
{
  #define IA32_APIC_BASE MSR = 0x1Bu;
  unsigned apic_result = 0;

  __asm__ volatile("rdmsr\n"
		   "mov	%%eax, %1\n" : "=ecx"(IA32_APIC_BASE_MSR),
                                       "=m"(apic_result) :: "eax", "edx");

  if( (apic_result & (1u << 8)) != 0 )
  {
    kprintf("Base processor\n");
  }

  if( (apic_result & (1u << 11)) != 0 )
  {
    kprintf("APIC enabled\n");
  }
  else
  {
    kprintf("APIC disabled\n");
  }

  kprintf("APIC Base: 0x%x\n", apic_result & ~0xFFFu);

  // TODO: Here, actually *set up* the LAPIC
  kMapPage( (addr_t)LAPIC_VADDR, (addr_t)LAPIC_BASE, PAGING_RW | PAGING_PCD | PAGING_PWT );

  // TODO: Setup the SVT
}
