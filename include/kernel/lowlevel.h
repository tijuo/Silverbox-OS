#ifndef KERNEL_LOWLEVEL_H
#define KERNEL_LOWLEVEL_H

#include <types.h>
#include <kernel/multiboot.h>
#include <kernel/bits.h>

#define MODE_BIT_WIDTH  32

#define KCODE           0x08u
#define KDATA           0x10u
#define UCODE           (0x18u | 0x03u)
#define UDATA           (0x20u | 0x03u)
#define TSS             0x28u
#define BKCODE          0x30u
#define BKDATA          0x38u

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
#define FAR_PTR(seg, off) ((((seg) & 0xFFFFu) << 4) + ((off) & 0xFFFFu))

#define IRQ_BASE	0x20

#define IRQ(num) (IRQ_BASE + (uint8_t)num)

#define EFLAGS_DEFAULT	0x02
#define EFLAGS_CF	(1 << 0)
#define EFLAGS_RESD (1 << 1)
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

#define CR4_OSXSAVE (1 << 18)

#define PCID_BITS   12
#define PCID_MASK   0xFFFu

#define SYSENTER_CS_MSR     0x174u
#define SYSENTER_ESP_MSR    0x175u
#define SYSENTER_EIP_MSR    0x176u

#define enableInt()     __asm__ __volatile__("sti\n")
#define disableInt()    __asm__ __volatile__("cli\n")

//#define IOMAP_LAZY_OFFSET	0xFFFF

/** Represents an entire TSS */

struct TSS_Struct
{
	uint16_t	backlink;
	uint16_t	_resd1;
	uint32_t	esp0;
	uint16_t	ss0;
	uint16_t	_resd2;
	uint32_t	esp1;
	uint16_t	ss1;
	uint16_t	_resd3;
	uint32_t	esp2;
	uint16_t	ss2;
	uint16_t	_resd4;
	uint32_t	cr3;
	uint32_t	eip;
	uint32_t	eflags;
	uint32_t	eax;
	uint32_t 	ecx;
	uint32_t	edx;
	uint32_t	ebx;
	uint32_t	esp;
	uint32_t	ebp;
	uint32_t	esi;
	uint32_t	edi;
	uint16_t	es;
	uint16_t	_resd5;
	uint16_t	cs;
	uint16_t	_resd6;
	uint16_t	ss;
	uint16_t	_resd7;
	uint16_t	ds;
	uint16_t	_resd8;
	uint16_t	fs;
	uint16_t	_resd9;
	uint16_t	gs;
	uint16_t	_resd10;
	uint16_t	ldt;
	uint16_t	_resd11;
	uint16_t	trap : 1;
	uint16_t	_resd12 : 15;
	uint16_t	ioMap;
} PACKED;

// 48 bytes

typedef struct
{
  uint32_t edi;
  uint32_t esi;
  uint32_t ebp;
  uint32_t ebx;
  uint32_t edx;
  uint32_t ecx;
  uint32_t eax;
  uint32_t eip;
  uint16_t cs;
  uint16_t avail;
  uint32_t eflags;
  uint32_t userEsp;
  uint16_t userSS;
  uint16_t avail2;
} ExecutionState;

// 288 bytes

typedef struct
{
  uint16_t fcw;
  uint16_t fsw;
  uint8_t ftw;
  uint8_t _resd1;
  uint16_t fop;
  uint32_t fpuIp;
  uint16_t fpuCs;
  uint16_t _resd2;

  uint32_t fpuDp;
  uint16_t fpuDs;
  uint16_t _resd3;
  uint32_t mxcsr;
  uint32_t mxcsrMask;

  union {
    struct MMX_Register {
      uint64_t value;
      uint64_t _resd;
    } mm[8];

    struct FPU_Register {
      uint8_t value[10];
      uint8_t _resd[6];
    } st[8];
  };

  struct XMM_Register {
    uint64_t low;
    uint64_t high;
  } xmm[8];

  // Even though the area is reserved, the processor won't access the reserved bits in 32-bit mode
  /*
  uint8_t _resd12[176];
  uint8_t available[48];
  */
} xsave_state_t;

extern void atomicInc( volatile void * );
extern void atomicDec( volatile void *);
extern int testAndSet( volatile void *, volatile int );

static inline void setCR0(uint32_t newCR0)
{
  __asm__("mov %0, %%cr0" :: "r"(newCR0));
}

static inline void setCR3(uint32_t newCR3)
{
  __asm__("mov %0, %%cr3" :: "r"(newCR3));
}

static inline void setCR4(uint32_t newCR4)
{
  __asm__("mov %0, %%cr4" :: "r"(newCR4));
}

static inline void setEflags(uint32_t newEflags)
{
  __asm__("pushl %0\n"
          "popf\n" :: "r"(newEflags) : "flags");
}

static inline uint32_t getCR0(void)
{
  uint32_t cr0;

  __asm__("mov %%cr0, %0" : "=r"(cr0));
  return cr0;
}

static inline uint32_t getCR2(void)
{
  uint32_t cr2;

  __asm__("mov %%cr2, %0" : "=r"(cr2));
  return cr2;
}

static inline uint32_t getCR3(void)
{
  uint32_t cr3;

  __asm__("mov %%cr3, %0" : "=r"(cr3));
  return cr3;
}

static inline uint32_t getCR4(void)
{
  uint32_t cr4;

  __asm__("mov %%cr4, %0" : "=r"(cr4));
  return cr4;
}

static inline uint32_t getEflags(void)
{
  uint32_t eflags;

  __asm__("pushf\n"
          "popl %0\n" : "=r"(eflags));

  return eflags;
}

static inline bool isXSaveSupported(void)
{
  return IS_FLAG_SET(getCR4(), CR4_OSXSAVE);
}

static inline bool intIsEnabled(void)
{
  return IS_FLAG_SET(getEflags(), EFLAGS_IF);
}

extern void loadGDT( void );

extern void cpuEx0Handler( void );
extern void cpuEx1Handler( void );
extern void cpuEx2Handler( void );
extern void cpuEx3Handler( void );
extern void cpuEx4Handler( void );
extern void cpuEx5Handler( void );
extern void cpuEx6Handler( void );
extern void cpuEx7Handler( void );
extern void cpuEx8Handler( void );
extern void cpuEx9Handler( void );
extern void cpuEx10Handler( void );
extern void cpuEx11Handler( void );
extern void cpuEx12Handler( void );
extern void cpuEx13Handler( void );
extern void cpuEx14Handler( void );
extern void cpuEx15Handler( void );
extern void cpuEx16Handler( void );
extern void cpuEx17Handler( void );
extern void cpuEx18Handler( void );
extern void cpuEx19Handler( void );
extern void cpuEx20Handler( void );
extern void cpuEx21Handler( void );
extern void cpuEx22Handler( void );
extern void cpuEx23Handler( void );
extern void cpuEx24Handler( void );
extern void cpuEx25Handler( void );
extern void cpuEx26Handler( void );
extern void cpuEx27Handler( void );
extern void cpuEx28Handler( void );
extern void cpuEx29Handler( void );
extern void cpuEx30Handler( void );
extern void cpuEx31Handler( void );

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
extern void irq16Handler( void );
extern void irq17Handler( void );
extern void irq18Handler( void );
extern void irq19Handler( void );
extern void irq20Handler( void );
extern void irq21Handler( void );
extern void irq22Handler( void );
extern void irq23Handler( void );
extern void irq24Handler( void );
extern void irq25Handler( void );
extern void irq26Handler( void );
extern void irq27Handler( void );
extern void irq28Handler( void );
extern void irq29Handler( void );
extern void irq30Handler( void );
extern void irq31Handler( void );

extern void irqHandler( void );

extern void syscallHandler( void );
extern void invalidIntHandler( void );

#define EXT_PTR(var)    (void *)( &var )

extern const void * const kCode;
extern const void * const kData;
extern const void * const kBss;
extern const void * const kEnd;
extern const void * const kTcbStart;
extern const void * const kTcbEnd;
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

extern const void * const kernelStacksTop;
extern const void * const kernelStacksBottom;

extern const unsigned int kernelGDT;
extern const unsigned int kernelIDT;
extern const unsigned int kernelTSS;

extern const unsigned int initKrnlPDir;
extern const unsigned int lowMemPageTable;
extern const unsigned int k1To1PageTable;
extern const unsigned int bootStackTop;

extern const unsigned int kCodeSel;
extern const unsigned int kDataSel;
extern const unsigned int uCodeSel;
extern const unsigned int uDataSel;
extern const unsigned int kTssSel;

extern unsigned int tssEsp0;
extern const unsigned int kVirtToPhys;
extern const unsigned int kPhysToVirt;
extern const unsigned int VPhysMemStart;
extern const unsigned int ioPermBitmap;

extern const size_t kernelStackLen;
#endif /* KERNEL_LOWLEVEL_H */
