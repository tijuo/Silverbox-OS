#ifndef KERNEL_LOWLEVEL_H
#define KERNEL_LOWLEVEL_H

#include <types.h>
#include <kernel/multiboot.h>

#define MODE_BIT_WIDTH  32

#define KCODE           0x08u
#define KDATA           0x10u
#define UCODE           (0x18u | 0x03u)
#define UDATA           (0x20u | 0x03u)
#define TSS             0x28u
#define BKCODE		    0x30u
#define BKDATA		    0x38u

#define TRAP_GATE       (I_TRAP << 16)
#define INT_GATE        (I_INT  << 16)
#define TASK_GATE       (I_TASK << 16)

#define I_TRAP          0x0Fu
#define I_INT           0x0Eu
#define I_TASK          0x05u

#define I_DPL0           0x00u
#define I_DPL1           ( 0x01u << 21 )
#define I_DPL2           ( 0x02u << 21 )
#define I_DPL3           ( 0x03u << 21 )

#define G_DPL0          0x00u
#define G_DPL1          0x20u
#define G_DPL2          0x40u
#define G_DPL3          0x60u

#define G_GRANULARITY   0x800u
#define G_BIG           0x400u

#define G_CONFORMING    0x04u
#define G_EXPUP         0x00u
#define G_EXPDOWN       0x04u

#define G_EXREAD        0x02u
#define G_EXONLY        0x00u

#define G_RDWR          0x02u
#define G_RDONLY        0x00u

#define G_ACCESSED      0x01u

#define G_CODESEG       0x18u
#define G_DATASEG       0x10u

#define G_PRESENT       0x80u

#define FP_SEG(addr)    (((addr_t)(addr) & 0xF0000u) >> 4)
#define FP_OFF(addr)    ((addr_t)(addr) & 0xFFFFu)

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

#define EFLAGS_DEFAULT	0x02
#define EFLAGS_CF	(1 << 0)
#define EFLAGS_PF	(1 << 2)
#define EFLAGS_AF	(1 << 4)
#define EFLAGS_ZF	(1 << 6)
#define EFLAGS_SF	(1 << 7)
#define EFLAGS_TF	(1 << 8)
#define EFLAGS_IF	(1 << 9)
#define EFLAGS_DF	(1 << 10)
#define EFLAGS_OF	(1 << 11)
#define EFLAGS_IOPL0	0
#define EFLAGS_IOPL1	(1 << 12)
#define EFLAGS_IOPL2	(2 << 12)
#define EFLAGS_IOPL3	(3 << 12)
#define EFLAGS_NT	(1 << 14)
#define EFLAGS_RF	(1 << 16)
#define EFLAGS_VM	(1 << 17)
#define EFLAGS_AC	(1 << 18)
#define EFLAGS_VIF	(1 << 19)
#define EFLAGS_VIP	(1 << 20)
#define EFLAGS_ID	(1 << 21)

#define PCID_BITS   12
#define PCID_MASK   0xFFFu

#define enableInt()     __asm__ __volatile__("sti\n")
#define disableInt()    __asm__ __volatile__("cli\n")

//#define IOMAP_LAZY_OFFSET	0xFFFF

/** Represents an entire TSS */

struct TSS_Struct
{
	word	backlink;
	word	_resd1;
	dword	esp0;
	word	ss0;
	word	_resd2;
	dword	esp1;
	word	ss1;
	word	_resd3;
	dword	esp2;
	word	ss2;
	word	_resd4;
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
	word	es;
	word	_resd5;
	word	cs;
	word	_resd6;
	word	ss;
	word	_resd7;
	word	ds;
	word	_resd8;
	word	fs;
	word	_resd9;
	word	gs;
	word	_resd10;
	word	ldt;
	word	_resd11;
	word	trap : 1;
	word	_resd12 : 15;
	word	ioMap;
} __PACKED__;

typedef struct
{
  dword edi;
  dword esi;
  dword ebp;
  dword ebx;
  dword edx;
  dword ecx;
  dword eax;
  dword eip;
  dword cs;
  dword eflags;
  dword userEsp;
  dword userSS;
} ExecutionState;

typedef struct
{
  word fcw;
  word fsw;
  byte ftw;
  byte _resd1;
  word fop;
  dword fpuIp;
  word cs;
  word _resd2;
  dword fpu_dp;
  word ds;
  word _resd3;
  dword _mxcsr;
  dword mxcsv_mask;

  byte mm0[10];
  byte _resd4[6];

  word mm1[10];
  byte _resd5[6];

  word mm2[10];
  byte _resd6[6];

  word mm3[10];
  byte _resd7[6];

  word mm4[10];
  byte _resd8[6];

  word mm5[10];
  byte _resd9[6];

  word mm6[10];
  byte _resd10[6];

  word mm7[10];
  byte _resd11[6];

  byte xmm0[16];
  byte xmm1[16];
  byte xmm2[16];
  byte xmm3[16];
  byte xmm4[16];
  byte xmm5[16];
  byte xmm6[16];
  byte xmm7[16];
  byte xmm8[16];
  byte xmm9[16];
  byte xmm10[16];
  byte xmm11[16];
  byte xmm12[16];
  byte xmm13[16];
  byte xmm14[16];
  byte xmm15[16];
  byte xmm16[16];

  byte _resd12[48];
  byte _avail[48];

} XSaveState;

typedef struct
{
  byte ymm0[16];
  byte ymm1[16];
  byte ymm2[16];
  byte ymm3[16];
  byte ymm4[16];
  byte ymm5[16];
  byte ymm6[16];
  byte ymm7[16];
  byte ymm8[16];
  byte ymm9[16];
  byte ymm10[16];
  byte ymm11[16];
  byte ymm12[16];
  byte ymm13[16];
  byte ymm14[16];
  byte ymm15[16];
  byte ymm16[16];

} XSaveYMMState;

extern void atomicInc( volatile void * );
extern void atomicDec( volatile void *);
extern int testAndSet( volatile void *, volatile int );

extern void setCR0( dword );
extern void setCR3( dword );
extern void setCR4( dword );
extern void setEflags( dword );
extern dword getCR0( void );
extern dword getCR2( void );
extern dword getCR3( void ) __attribute__((fastcall));
extern dword getCR4( void ) ;
extern dword getEflags( void );
extern bool intIsEnabled( void );

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

//extern multiboot_info_t *getMultibootInfo( void );

#define EXT_PTR(var)    (void *)( &var )

extern const void * const kCode;
extern const void * const kData;
extern const void * const kBss;
extern const void * const kEnd;
extern const void * const kTcbStart;
extern const void * const kPhysStart;
extern const void * const kVirtStart;
extern const void * const kdCode;
extern const void * const kdData;
extern const void * const kPhysData;
extern const void * const kVirtData;
extern const void * const kPhysBss;
extern const void * const kVirtBss;
extern const void * const kSize;
extern const void * const kTcbTableSize;

extern const void * const kVirtPageStack;

extern const unsigned int kernelGDT;
extern const unsigned int kernelIDT;
extern const unsigned int kernelTSS;

extern const unsigned int initKrnlPDir;
extern const unsigned int lowMemPageTable;
extern const unsigned int k1To1PageTable;
extern const unsigned int bootStackTop;
extern const unsigned int kernelStackTop;

extern const unsigned int kCodeSel;
extern const unsigned int kDataSel;
extern const unsigned int uCodeSel;
extern const unsigned int uDataSel;
extern const unsigned int kTssSel;

extern const unsigned int kernelStack;
extern const unsigned int tssEsp0;
extern const unsigned int kVirtToPhys;
extern const unsigned int kPhysToVirt;
extern const unsigned int VPhysMemStart;
extern const unsigned int ioPermBitmap;

extern const size_t kernelStackLen;
#endif /* KERNEL_LOWLEVEL_H */
