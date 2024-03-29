#ifndef KERNEL_LOWLEVEL_H
#define KERNEL_LOWLEVEL_H

#include <types.h>
#include <kernel/bits.h>

#define MODE_BIT_WIDTH      32u

#define RING0_DPL           0u
#define RING1_DPL           1u
#define RING2_DPL           2u
#define RING3_DPL           3u

#define KCODE_SEL           (0x08u | RING0_DPL)
#define KDATA_SEL           (0x10u | RING0_DPL)
#define UCODE_SEL           (0x18u | RING3_DPL)
#define UDATA_SEL           (0x20u | RING3_DPL)
#define TSS_SEL             0x28u
#define BKCODE_SEL          (0x30u | RING0_DPL)
#define BKDATA_SEL          (0x38u | RING0_DPL)

#define TRAP_GATE       (I_TRAP << 16)
#define INT_GATE        (I_INT  << 16)
#define TASK_GATE       (I_TASK << 16)

#define I_TRAP          0x0Fu
#define I_INT           0x0Eu
#define I_TASK          0x05u

#define I_DPL0           ( RING0_DPL << 21 )
#define I_DPL1           ( RING1_DPL << 21 )
#define I_DPL2           ( RING2_DPL << 21 )
#define I_DPL3           ( RING3_DPL << 21 )

#define GDT_DPL0           ( RING0_DPL << 5 )
#define GDT_DPL1           ( RING1_DPL << 5 )
#define GDT_DPL2           ( RING2_DPL << 5 )
#define GDT_DPL3           ( RING3_DPL << 5 )

#define GDT_BYTE_GRAN     0x0u
#define GDT_PAGE_GRAN     0x8u
#define GDT_BIG           0x4u
#define GDT_CODE64        0x2u

#define GDT_SYS           0x00u
#define GDT_NONSYS        0x10u

#define GDT_NONCONF       0x00u
#define GDT_CONF          0x04u

#define GDT_EXPUP         0x00u
#define GDT_EXPDOWN       0x04u

#define GDT_READ          0x02u
#define GDT_EXONLY        0x00u

#define GDT_RDWR          0x02u
#define GDT_RDONLY        0x00u

#define GDT_ACCESSED      0x01u

#define GDT_CODE          0x08u
#define GDT_DATA          0x00u

#define GDT_PRESENT       0x80u

#define GDT_TSS           0x09u

#define FP_SEG(addr)    (((addr_t)(addr) & 0xF0000u) >> 4)
#define FP_OFF(addr)    ((addr_t)(addr) & 0xFFFFu)
#define FAR_PTR(seg, off) ((((seg) & 0xFFFFu) << 4) + ((off) & 0xFFFFu))

#define IRQ_BASE	0x20u

#define IRQ(num) (IRQ_BASE + (uint8_t)num)

#define EFLAGS_DEFAULT	0x02u
#define EFLAGS_CF	(1u << 0)
#define EFLAGS_RESD (1u << 1)
#define EFLAGS_PF	(1u << 2)
#define EFLAGS_AF	(1u << 4)
#define EFLAGS_ZF	(1u << 6)
#define EFLAGS_SF	(1u << 7)
#define EFLAGS_TF	(1u << 8)
#define EFLAGS_IF	(1u << 9)
#define EFLAGS_DF	(1u << 10)
#define EFLAGS_OF	(1u << 11)
#define EFLAGS_IOPL0	0u
#define EFLAGS_IOPL1	(1u << 12)
#define EFLAGS_IOPL2	(2 << 12)
#define EFLAGS_IOPL3	(3 << 12)
#define EFLAGS_NT	(1u << 14)
#define EFLAGS_RF	(1u << 16)
#define EFLAGS_VM	(1u << 17)
#define EFLAGS_AC	(1u << 18)
#define EFLAGS_VIF	(1u << 19)
#define EFLAGS_VIP	(1u << 20)
#define EFLAGS_ID	(1u << 21)

#define CR0_PE          (1u << 0)
#define CR0_MP          (1u << 1)
#define CR0_EM          (1u << 2)
#define CR0_WP          (1u << 16)
#define CR0_AM          (1u << 18)
#define CR0_NW          (1u << 29)
#define CR0_CD          (1u << 30)
#define CR0_PG          (1u << 31)

#define CR3_PWT         (1u << 3)
#define CR3_PCD         (1u << 4)

#define CR4_DE          (1u << 3)
#define CR4_PSE         (1u << 4)
#define CR4_PAE         (1u << 5)
#define CR4_MCE         (1u << 6)
#define CR4_PGE         (1u << 7)
#define CR4_PCE         (1u << 8)
#define CR4_OSFXSR      (1u << 9)
#define CR4_OSXMMEXCPT  (1u << 10)
#define CR4_FSGSBASE    (1u << 16)
#define CR4_PCIDE       (1u << 17)
#define CR4_OSXSAVE     (1u << 18)

#define PCID_BITS   12
#define PCID_MASK   0xFFFu

#define SYSENTER_CS_MSR     0x174u
#define SYSENTER_ESP_MSR    0x175u
#define SYSENTER_EIP_MSR    0x176u

#define enableInt()     __asm__ __volatile__("sti" ::: "cc")
#define disableInt()    __asm__ __volatile__("cli" ::: "cc")
#define halt()          __asm__ __volatile__("hlt")

//#define IOMAP_LAZY_OFFSET	0xFFFF

union InterruptStackFrame {
  struct {
    uint32_t eip;
    uint16_t cs;
    uint16_t _resd;
    uint32_t eflags;
    uint16_t ss;
    uint16_t _resd2;
    uint32_t esp;
  };

  struct {
    uint32_t eip;
    uint16_t cs;
    uint16_t _resd;
    uint32_t eflags;
  } samePriv;

  struct {
    uint32_t eip;
    uint16_t cs;
    uint16_t _resd;
    uint32_t eflags;
    uint16_t ss;
    uint16_t _resd2;
    uint32_t esp;
  } changePriv;
};

typedef union InterruptStackFrame interrupt_frame_t;

/** Represents an entire TSS */

struct TSS_Struct {
  uint16_t backlink;
  uint16_t _resd1;
  uint32_t esp0;
  uint16_t ss0;
  uint16_t _resd2;
  uint32_t esp1;
  uint16_t ss1;
  uint16_t _resd3;
  uint32_t esp2;
  uint16_t ss2;
  uint16_t _resd4;
  uint32_t cr3;
  uint32_t eip;
  uint32_t eflags;
  uint32_t eax;
  uint32_t ecx;
  uint32_t edx;
  uint32_t ebx;
  uint32_t esp;
  uint32_t ebp;
  uint32_t esi;
  uint32_t edi;
  uint16_t es;
  uint16_t _resd5;
  uint16_t cs;
  uint16_t _resd6;
  uint16_t ss;
  uint16_t _resd7;
  uint16_t ds;
  uint16_t _resd8;
  uint16_t fs;
  uint16_t _resd9;
  uint16_t gs;
  uint16_t _resd10;
  uint16_t ldt;
  uint16_t _resd11;
  uint16_t trap :1;
  uint16_t _resd12 :15;
  uint16_t ioMap;
  uint8_t tssIoBitmap[8088];
};

// 56 bytes

typedef struct {
  uint16_t gs;
  uint16_t fs;
  uint16_t es;
  uint16_t ds;
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

typedef struct {
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

union GdtEntry {
  struct {
    uint16_t limit1;
    uint16_t base1;
    uint8_t base2;
    uint8_t accessFlags;
    uint8_t limit2 :4;
    uint8_t flags2 :4;
    uint8_t base3;
  };

  uint64_t value;
};

typedef union GdtEntry gdt_entry_t;

_Static_assert(sizeof(gdt_entry_t) == 8, "GDTEntry should be 8 bytes");

struct GdtPointer {
  uint16_t limit;
  uint32_t base;
} PACKED;

struct IdtEntry {
  uint16_t offsetLower;
  uint16_t selector;
  uint8_t _resd;
  uint8_t gateType :4;
  uint8_t isStorage :1;
  uint8_t dpl :2;
  uint8_t isPresent :1;
  uint16_t offsetUpper;
};

typedef struct IdtEntry idt_entry_t;

_Static_assert(sizeof(idt_entry_t) == 8, "IDTEntry should be 8 bytes");

struct IdtPointer {
  uint16_t limit;
  uint32_t base;
} PACKED;

extern void atomicInc(volatile void*);
extern void atomicDec(volatile void*);
extern int testAndSet(volatile void*, volatile int);

static inline void setCR0(uint32_t newCR0) {
  __asm__("mov %0, %%cr0" :: "r"(newCR0));
}

static inline void setCR3(uint32_t newCR3) {
  __asm__("mov %0, %%cr3" :: "r"(newCR3) : "memory");
}

static inline void setCR4(uint32_t newCR4) {
  __asm__("mov %0, %%cr4" :: "r"(newCR4));
}

static inline void setEflags(uint32_t newEflags) {
  __asm__("pushl %0\n"
          "popf\n" :: "r"(newEflags) : "cc");
}

static inline uint32_t getCR0(void) {
  uint32_t cr0;

  __asm__("mov %%cr0, %0" : "=r"(cr0));
  return cr0;
}

static inline uint32_t getCR2(void) {
  uint32_t cr2;

  __asm__("mov %%cr2, %0" : "=r"(cr2));
  return cr2;
}

static inline uint32_t getCR3(void) {
  uint32_t cr3;

  __asm__("mov %%cr3, %0" : "=r"(cr3));
  return cr3;
}

static inline uint32_t getCR4(void) {
  uint32_t cr4;

  __asm__("mov %%cr4, %0" : "=r"(cr4));
  return cr4;
}

static inline uint32_t getEflags(void) {
  uint32_t eflags;

  __asm__("pushf\n"
          "popl %0\n" : "=r"(eflags));

  return eflags;
}

static inline uint16_t getDs(void) {
  uint16_t ds;

  __asm__("mov %%ds, %0" : "=r"(ds));

  return ds;
}

static inline uint16_t getEs(void) {
  uint16_t es;

  __asm__("mov %%es, %0" : "=r"(es));

  return es;
}

static inline uint16_t getFs(void) {
  uint16_t fs;

  __asm__("mov %%fs, %0" : "=r"(fs));

  return fs;
}

static inline uint16_t getGs(void) {
  uint16_t gs;

  __asm__("mov %%gs, %0" : "=r"(gs));

  return gs;
}

static inline uint16_t getSs(void) {
  uint16_t ss;

  __asm__("mov %%ss, %0" : "=r"(ss));

  return ss;
}

static inline void setCs(uint16_t cs) {
  __asm__("push %0\n"
          "push $1f\n"
          "retf\n"
          "1:\n" :: "r"(cs) : "memory");
}

static inline void setDs(uint16_t ds) {
  __asm__("mov %0, %%ds" :: "r"(ds) : "memory");
}

static inline void setEs(uint16_t es) {
  __asm__("mov %0, %%es" :: "r"(es) : "memory");
}

static inline void setFs(uint16_t fs) {
  __asm__("mov %0, %%fs" :: "r"(fs));
}

static inline void setGs(uint16_t gs) {
  __asm__("mov %0, %%gs" :: "r"(gs));
}

static inline void setSs(uint16_t ss) {
  __asm__("mov %0, %%ss" :: "r"(ss) : "memory");
}

static inline bool isXSaveSupported(void) {
  return IS_FLAG_SET(getCR4(), CR4_OSXSAVE);
}

static inline bool intIsEnabled(void) {
  return IS_FLAG_SET(getEflags(), EFLAGS_IF);
}

static inline void wrmsr(uint32_t msr, uint64_t data) {
  uint32_t eax = (uint32_t)data;
  uint32_t edx = (uint32_t)(data >> 32);

  __asm__ __volatile__("wrmsr" :: "c"(msr), "a"(eax), "d"(edx));
}

static inline uint64_t rdmsr(uint32_t msr) {
  uint32_t eax;
  uint32_t edx;

  __asm__ __volatile__("rdmsr" : "=a"(eax), "=d"(edx) : "c"(msr));

  return ((uint64_t)edx << 32) | (uint64_t)eax;
}

extern void loadGDT(void);

#define EXT_PTR(var)    (const void * const)( &var )

extern const void *const kCode;
extern const void *const kData;
extern const void *const kBss;
extern const void *const kEnd;
extern const void *const kTcbStart;
extern const void *const kTcbEnd;
extern const void *const kPhysStart;
extern const void *const kVirtStart;
extern const void *const kdCode;
extern const void *const kdData;
extern const void *const kPhysData;
extern const void *const kVirtData;
extern const void *const kPhysBss;
extern const void *const kVirtBss;
extern const void *const kSize;
extern const void *const kTcbTableSize;

extern const void *const kVirtPageStack;

/*
 extern const void *const kernelStacksTop;
 extern const void *const kernelStacksBottom;

 extern const unsigned int kernelGDT;
 extern const unsigned int kernelIDT;
 extern const unsigned int kernelTSS;


 extern const unsigned int initKrnlPDir;
 extern const unsigned int lowMemPageTable;
 extern const unsigned int k1To1PageTable;
 extern const unsigned int bootStackTop;
 */

extern const unsigned int kCodeSel;
extern const unsigned int kDataSel;
extern const unsigned int uCodeSel;
extern const unsigned int uDataSel;
extern const unsigned int kTssSel;

//extern unsigned int tssEsp0;
extern const unsigned int kVirtToPhys;
extern const unsigned int kPhysToVirt;
extern const unsigned int VPhysMemStart;

extern struct TSS_Struct tss;

#define SAVE_STATE \
__asm__ ( \
          "push %%eax\n" \
          "push %%ecx\n" \
          "push %%edx\n" \
          "push %%ebx\n" \
          "push %%ebp\n" \
          "push %%esi\n" \
          "push %%edi\n" \
          "mov %%ds, %%ax\n" \
          "shl $16, %%eax\n" \
          "mov %%es, %%ax\n" \
          "push %%eax\n" \
          "mov %%fs, %%ax\n" \
          "shl $16, %%eax\n" \
          "mov %%gs, %%ax\n" \
          "push %%eax\n" \
          "lea tss, %%eax\n" \
          "lea 4(%%eax), %%eax\n" \
          "pushl (%%eax)\n" \
          "cmpl $0, (%%eax)\n" /* Is tss.esp0 NULL? (because exception occurred during init()) */ \
          "je 1f\n" \
          "mov %%esp, (%%eax)\n" \
          "mov %%ss, %%cx\n" \
          "cmpw 54(%%esp), %%cx\n" /* Privilege Change? */ \
          "je 2f\n" \
          "lea kernelStackTop, %%ecx\n" \
          "mov %%ecx, (%%eax)\n" /* (user-> kernel switch) Set tss kernel stack ptr as top of kernel stack. */ \
          "jmp 1f\n" \
          "2:\n" /* No privilege change (kernel->kernel switch) */\
          "mov %%esp, (%%eax)\n" \
          "1:\n" \
          ::: "eax", "ecx", "memory", "cc" \
)

#define SAVE_ERR_STATE \
__asm__ ( \
          "push %%ecx\n" \
          "mov 4(%%esp), %%ecx\n" /* Save error code to ecx */ \
          "mov %%eax, 4(%%esp)\n" /* Put eax where the error code used to be. */ \
          "push %%edx\n" \
          "push %%ebx\n" \
          "push %%ebp\n" \
          "push %%esi\n" \
          "push %%edi\n" \
          "mov %%ds, %%ax\n" \
          "shl $16, %%eax\n" \
          "mov %%es, %%ax\n" \
          "push %%eax\n" \
          "mov %%fs, %%ax\n" \
          "shl $16, %%eax\n" \
          "mov %%gs, %%ax\n" \
          "push %%eax\n" \
          "lea tss, %%eax\n" \
          "lea 4(%%eax), %%eax\n" \
          "pushl (%%eax)\n" \
          "cmpl $0, (%%eax)\n" /* Is tssEsp0 NULL? (because exception occurred during init()) */ \
          "je 1f\n" /* If so, don't bother saving/loading tssEsp0 */ \
          "mov %%esp, (%%eax)\n" \
          "mov %%ss, %%cx\n" \
          "cmpw 54(%%esp), %%cx\n" /* Privilege Change? */ \
          "je 2f\n" \
          "lea kernelStackTop, %%edx\n" \
          "mov %%edx, (%%eax)\n" /* (user-> kernel switch) Set tss kernel stack ptr as top of kernel stack. */ \
          "jmp 1f\n" \
          "2:\n" /* No privilege change (kernel->kernel switch) */\
          "mov %%esp, (%%eax)\n" \
          "1:\n" \
          "push %%ecx\n" \
          ::: "eax", "ecx", "edx", "memory", "cc" \
)

#define RESTORE_STATE \
__asm__( \
         "lea tss, %%eax\n" \
         "mov 4(%%eax), %%esp\n" \
         "pop %%ecx\n" \
         "mov %%ecx, 4(%%eax)\n" \
         "pop %%eax\n" \
         "mov %%ax, %%gs\n" \
         "shr $16, %%eax\n" \
         "mov %%ax, %%fs\n" \
         "pop %%eax\n" \
         "mov %%ax, %%es\n" \
         "shr $16, %%eax\n" \
         "mov %%ax, %%ds\n" \
         "pop %%edi\n" \
         "pop %%esi\n" \
         "pop %%ebp\n" \
         "pop %%ebx\n" \
         "pop %%edx\n" \
         "pop %%ecx\n" \
         "pop %%eax\n" \
         "iret\n" \
         ::: "eax", "ebx", "ecx", "edx", "esi", "edi", "memory", "cc" \
)


extern void cpuEx0Handler(void);
extern void cpuEx1Handler(void);
extern void cpuEx2Handler(void);
extern void cpuEx3Handler(void);
extern void cpuEx4Handler(void);
extern void cpuEx5Handler(void);
extern void cpuEx6Handler(void);
extern void cpuEx7Handler(void);
extern void cpuEx8Handler(void);
extern void cpuEx9Handler(void);
extern void cpuEx10Handler(void);
extern void cpuEx11Handler(void);
extern void cpuEx12Handler(void);
extern void cpuEx13Handler(void);
extern void cpuEx14Handler(void);
extern void cpuEx15Handler(void);
extern void cpuEx16Handler(void);
extern void cpuEx17Handler(void);
extern void cpuEx18Handler(void);
extern void cpuEx19Handler(void);
extern void cpuEx20Handler(void);
extern void cpuEx21Handler(void);
extern void cpuEx22Handler(void);
extern void cpuEx23Handler(void);
extern void cpuEx24Handler(void);
extern void cpuEx25Handler(void);
extern void cpuEx26Handler(void);
extern void cpuEx27Handler(void);
extern void cpuEx28Handler(void);
extern void cpuEx29Handler(void);
extern void cpuEx30Handler(void);
extern void cpuEx31Handler(void);

extern void irq0Handler(void);
extern void irq1Handler(void);
extern void irq2Handler(void);
extern void irq3Handler(void);
extern void irq4Handler(void);
extern void irq5Handler(void);
extern void irq6Handler(void);
extern void irq7Handler(void);
extern void irq8Handler(void);
extern void irq9Handler(void);
extern void irq10Handler(void);
extern void irq11Handler(void);
extern void irq12Handler(void);
extern void irq13Handler(void);
extern void irq14Handler(void);
extern void irq15Handler(void);
extern void irq16Handler(void);
extern void irq17Handler(void);
extern void irq18Handler(void);
extern void irq19Handler(void);
extern void irq20Handler(void);
extern void irq21Handler(void);
extern void irq22Handler(void);
extern void irq23Handler(void);

#endif /* KERNEL_LOWLEVEL_H */
