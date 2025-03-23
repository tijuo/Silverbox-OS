#include <kernel/apic.h>
#include <kernel/mm.h>
#include <kernel/debug.h>
#include <kernel/lowlevel.h>

void send_apic_eoi(void);

uint32_t lapic_ptr;
uint32_t ioapic_ptr;

/* APIC memory must be mapped as strong uncachable(UC)
 - page 428 of IA32_SDM_Vol3A */
void send_apic_eoi(void)
{
    apic_ptr_t eoi_reg = LAPIC_REG(LAPIC_EOI);
    *eoi_reg = 0;
}