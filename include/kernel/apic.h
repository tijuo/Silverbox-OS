#ifndef APIC_H
#define APIC_H

#include <stdint.h>

// Memory mapped IOAPIC registers

// I/O register select register
#define IOAPIC_IOREGSEL     0xFEC00000u

// I/O window register
#define IOAPIC_IOWIN        0xFEC00010u

// IOAPIC registers

#define IOAPIC_ID            0x00
#define IOAPIC_VER           0x01
#define IOAPIC_ARB           0x02

// IOAPIC redirection table (64-bit)

#define IOAPIC_REDTBL0       0x10
#define IOAPIC_REDTBL1       0x12
#define IOAPIC_REDTBL2       0x14
#define IOAPIC_REDTBL3       0x16
#define IOAPIC_REDTBL4       0x18
#define IOAPIC_REDTBL5       0x1A
#define IOAPIC_REDTBL6       0x1C
#define IOAPIC_REDTBL7       0x1E
#define IOAPIC_REDTBL8       0x20
#define IOAPIC_REDTBL9       0x22
#define IOAPIC_REDTBL10      0x24
#define IOAPIC_REDTBL11      0x26
#define IOAPIC_REDTBL12      0x28
#define IOAPIC_REDTBL13      0x2A
#define IOAPIC_REDTBL14      0x2C
#define IOAPIC_REDTBL15      0x2E
#define IOAPIC_REDTBL16      0x30
#define IOAPIC_REDTBL17      0x32
#define IOAPIC_REDTBL18      0x34
#define IOAPIC_REDTBL19      0x36
#define IOAPIC_REDTBL20      0x38
#define IOAPIC_REDTBL21      0x3A
#define IOAPIC_REDTBL22      0x3C
#define IOAPIC_REDTBL23      0x3E

#define LAPIC_OFFSET	    0
#define LAPIC_BASE	        0xFEE00000

#define LAPIC_ID	        0x20
#define LAPIC_VERSION	    0x30
#define LAPIC_TPR	        0x80
#define LAPIC_APR	        0x90
#define LAPIC_PPR	        0xA0
#define LAPIC_EOI	        0xB0

#define LAPIC_ISR0	        0x100
#define LAPIC_ISR1          0x110
#define LAPIC_ISR2          0x120
#define LAPIC_ISR3          0x130
#define LAPIC_ISR4          0x140
#define LAPIC_ISR5          0x150
#define LAPIC_ISR6          0x160
#define LAPIC_ISR7          0x170

#define LAPIC_TMR0          0x180
#define LAPIC_TMR1          0x190
#define LAPIC_TMR2          0x1A0
#define LAPIC_TMR3          0x1B0
#define LAPIC_TMR4          0x1C0
#define LAPIC_TMR5          0x1D0
#define LAPIC_TMR6          0x1E0
#define LAPIC_TMR7          0x1F0

#define LAPIC_IRR0          0x200
#define LAPIC_IRR1          0x210
#define LAPIC_IRR2          0x220
#define LAPIC_IRR3          0x230
#define LAPIC_IRR4          0x240
#define LAPIC_IRR5          0x250
#define LAPIC_IRR6          0x260
#define LAPIC_IRR7          0x270

#define LAPIC_ERROR	        0x280

#define LAPIC_ICR0	        0x300
#define LAPIC_ICR1	        0x310
#define LAPIC_TIMER	        0x320

#define LAPIC_TIMER_IC	    0x380
#define LAPIC_TIMER_CC	    0x390
#define LAPIC_TIMER_DCR	    0x3E0

#define LAPIC_ONESHOT	      0
#define LAPIC_PERIODIC	    (1u << 17)
#define LAPIC_MASKED	      (1u << 18)
#define LAPIC_UNMASKED	    0

#define IA32_APIC_BASE_MSR 0x1Bu

extern uint32_t lapic_ptr;
extern uint32_t ioapic_ptr;
#endif /* APIC_H */