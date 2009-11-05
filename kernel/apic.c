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
 // IA32_APIC_BASE MSR = 0x1B
  unsigned apic_result = 0;

  __asm__ volatile("mov $0x1B, %%ecx\n"
		   "rdmsr\n"
		   "mov	%%eax, %0\n" : "=m"(apic_result) :: "ecx");

  if( apic_result & (1 << 8) )
    kprintf("Base processor\n");

  if(apic_result & (1 << 11) )
    kprintf("APIC enabled\n");
  else
    kprintf("APIC disabled\n");

  kprintf("APIC Base: 0x%x\n", apic_result & ~0xFFF);  

  // TODO: Here, actually *set up* the APIC
  kMapPage( (addr_t)LAPIC_VADDR, (addr_t)LAPIC_BASE, PAGING_RW | PAGING_PCD );
}
