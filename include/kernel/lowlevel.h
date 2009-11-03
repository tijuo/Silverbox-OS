#ifndef LOWLEVEL_H
#define LOWLEVEL_H

#include <types.h>
#include <kernel/pit.h>
#include <kernel/multiboot.h>

#define KCODE           0x08
#define KDATA           0x10
#define UCODE           (0x18 | 0x03)
#define UDATA           (0x20 | 0x03)
#define TSS             0x28
#define BKCODE		    0x30
#define BKDATA		    0x38

#define TRAP_GATE       (I_TRAP << 16)
#define INT_GATE        (I_INT  << 16)
#define TASK_GATE       (I_TASK << 16)

#define I_TRAP          0x0F
#define I_INT           0x0E
#define I_TASK          0x05

#define I_DPL0           0x00
#define I_DPL1           ( 0x01 << 21 )
#define I_DPL2           ( 0x02 << 21 )
#define I_DPL3           ( 0x03 << 21 )

#define G_DPL0          0x00
#define G_DPL1          0x20
#define G_DPL2          0x40
#define G_DPL3          0x60

#define G_GRANULARITY   0x800
#define G_BIG           0x400

#define G_CONFORMING    0x04
#define G_EXPUP         0x00
#define G_EXPDOWN       0x04

#define G_EXREAD        0x02
#define G_EXONLY        0x00

#define G_RDWR          0x02
#define G_RDONLY        0x00

#define G_ACCESSED      0x01

#define G_CODESEG       0x18
#define G_DATASEG       0x10

#define G_PRESENT       0x80

#define FP_SEG(addr)    (((addr_t)addr & 0xF0000) >> 4)
#define FP_OFF(addr)    ((addr_t)addr & 0xFFFF)

// #define KERNEL_IDT 	(&kernelGDT + PHYSMEM_START)
// #define KERNEL_GDT 	(&kernelIDT + PHYSMEM_START)

#define IRQ0            0x20
#define IRQ1            (IRQ0 + 1)
#define IRQ2            (IRQ0 + 2)
#define IRQ3            (IRQ0 + 3)
#define IRQ4            (IRQ0 + 4)
#define IRQ5            (IRQ0 + 5)
#define IRQ6            (IRQ0 + 6)
#define IRQ7            (IRQ0 + 7)
#define IRQ8            0x28
#define IRQ9            (IRQ8 + 1)
#define IRQ10           (IRQ8 + 2)
#define IRQ11           (IRQ8 + 3)
#define IRQ12           (IRQ8 + 4)
#define IRQ13           (IRQ8 + 5)
#define IRQ14           (IRQ8 + 6)
#define IRQ15           (IRQ8 + 7)

#define enableInt()     asm __volatile__("sti\n")
#define disableInt()    asm __volatile__("cli\n")

struct TSS_Struct
{
	dword	backlink;
	dword	esp0;
	dword	ss0;
	dword	esp1;
	dword	ss1;
	dword	esp2;
	dword	ss2;
	dword	cr3;
	dword	eip;
	dword	eflags;
	dword	eax;
	dword 	ecx;
	dword	edx;
	dword	ebx;
	dword	esp;
	dword	ebp;
	dword	esi;
	dword	edi;
	dword	es;
	dword	cs;
	dword	ss;
	dword	ds;
	dword	fs;
	dword	gs;
	dword	ldt;
	dword	ioMap;
} __PACKED__;

/* This is eight bytes long */

struct IDT_Entry
{

} __PACKED__;

extern void atomicInc( volatile void * );
extern void atomicDec( volatile void *);
extern volatile int testAndSet( volatile void *, volatile int );

extern void setCR0( volatile dword );
extern void setCR3( volatile dword );
extern void setCR4( volatile dword );
extern void setEflags( volatile dword );
extern dword getCR0( void );
extern dword getCR2( void );
extern dword getCR3( void ) __attribute__((fastcall));
extern dword getCR4( void ) ;
extern dword getEflags( void );
extern bool intIsEnabled( void );

extern void enableIRQ( int irq );
extern void disableIRQ( int irq );
extern void enableAllIRQ( void );
extern void disableAllIRQ( void );
extern void sendEOI( void );
extern void loadGDT( void );

extern void intHandler0( void );
extern void intHandler1( void );
extern void intHandler2( void );
extern void intHandler3( void );
extern void intHandler4( void );
extern void intHandler5( void );
extern void intHandler6( void );
extern void intHandler7( void );
extern void intHandler8( void );
extern void intHandler9( void );
extern void intHandler10( void );
extern void intHandler11( void );
extern void intHandler12( void );
extern void intHandler13( void );
extern void intHandler14( void );
extern void intHandler16( void );
extern void intHandler17( void );
extern void intHandler18( void );
extern void intHandler19( void );
extern void irq0Handler( void );
extern void irq1Handler( void );
extern void irq2Handler( void );
extern void irq3Handler( void );
extern void irq4Handler( void );
extern void irq5Handler( void );
extern void irq6Handler( void );
extern void irq7Handler( void );
extern void irq8Handler( void );
extern void irq9Handler( void );
extern void irq10Handler( void );
extern void irq11Handler( void );
extern void irq12Handler( void );
extern void irq13Handler( void );
extern void irq14Handler( void );
extern void irq15Handler( void );

extern void invalidIntHandler( void );
extern void loadIDT( void );
extern void syscallHandler( void );
extern void timerHandler( void );

extern void addIDTEntry( void *addr, uint32 num, uint32 flags);

extern multiboot_info_t *getMultibootInfo( void );

#define EXT_PTR(var)    (void *)( &var )

volatile addr_t tssIOBitmap;
volatile unsigned *tssEsp0;

extern char *kCode, *kData, *kBss, *kEnd, *kPhysStart, *kVirtStart;
extern char *kdCode, *kdData;
extern char *kPhysData, *kVirtData, *kPhysBss, *kVirtBss, *kSize;
extern char *kernelGDT, *kernelIDT, *kernelTSS;
extern char *kVirtToPhys, *kPhysToVirt, *VPhysMemStart;

#endif
