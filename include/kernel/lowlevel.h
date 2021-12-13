#ifndef KERNEL_LOWLEVEL_H
#define KERNEL_LOWLEVEL_H

#include <stdint.h>
#include <bits.h>

#define MODE_BIT_WIDTH      64u

#define RING0_DPL           0u
#define RING1_DPL           1u
#define RING2_DPL           2u
#define RING3_DPL           3u

#define KCODE_SEL           (0x08u | RING0_DPL)
#define KDATA_SEL           (0x10u | RING0_DPL)
#define UCODE_SEL           (0x18u | RING3_DPL)
#define UDATA_SEL           (0x20u | RING3_DPL)
#define TSS_SEL             0x28u

#define TRAP_GATE       (I_TRAP << 16)
#define INT_GATE        (I_INT  << 16)
#define TASK_GATE       (I_TASK << 16)

#define I_TRAP          0x0Fu
#define I_INT           0x0Eu
#define I_TASK          0x05u

#define I_DPL_BITS		5

#define I_PRESENT		(1 << 7)

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

#define IRQ_BASE	0x20u

#define IRQ(num) (IRQ_BASE + (num))

#define RFLAGS_DEFAULT	0x02u
#define RFLAGS_CF	(1u << 0)
#define RFLAGS_RESD (1u << 1)
#define RFLAGS_PF	(1u << 2)
#define RFLAGS_AF	(1u << 4)
#define RFLAGS_ZF	(1u << 6)
#define RFLAGS_SF	(1u << 7)
#define RFLAGS_TF	(1u << 8)
#define RFLAGS_IF	(1u << 9)
#define RFLAGS_DF	(1u << 10)
#define RFLAGS_OF	(1u << 11)
#define RFLAGS_IOPL0	0u
#define RFLAGS_IOPL1	(1u << 12)
#define RFLAGS_IOPL2	(2 << 12)
#define RFLAGS_IOPL3	(3 << 12)
#define RFLAGS_NT	(1u << 14)
#define RFLAGS_RF	(1u << 16)
#define RFLAGS_VM	(1u << 17)
#define RFLAGS_AC	(1u << 18)
#define RFLAGS_VIF	(1u << 19)
#define RFLAGS_VIP	(1u << 20)
#define RFLAGS_ID	(1u << 21)

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

#define SYSCALL_FMASK_MSR	0xC0000084
#define SYSCALL_STAR_MSR	0xC0000081
#define SYSCALL_LSTAR_MSR	0xC0000082

#define enable_int()     __asm__ __volatile__("sti" ::: "cc")
#define disable_int()   __asm__ __volatile__("cli" ::: "cc")
#define halt()          __asm__ __volatile__("hlt")

// 416 bytes

typedef struct {
  uint16_t fcw;
  uint16_t fsw;
  uint8_t ftw;
  uint8_t _resd1;
  uint16_t fop;
  uint32_t fpu_ip;
  uint16_t fpu_cs;
  uint16_t _resd2;

  uint32_t fpu_dp;
  uint16_t fpu_ds;
  uint16_t _resd3;
  uint32_t mxcsr;
  uint32_t mxcsr_mask;

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
  } xmm[16];


 uint8_t _resd12[48];
 uint8_t available[48];
} xsave_state_t;

//#define IOMAP_LAZY_OFFSET	0xFFFF

union InterruptStackFrame {
  struct {
	uint64_t rip;
	uint64_t cs;
	uint64_t rflags;
	uint64_t rsp;
	uint64_t ss;
  };
};

typedef union InterruptStackFrame interrupt_frame_t;

_Static_assert(sizeof(interrupt_frame_t) == 40, "interrupt_frame_t should be 40 bytes");

/** Represents an entire TSS */

struct tss_struct {
  uint32_t _resd;
  uint64_t rsp0;
  uint64_t rsp1;
  uint64_t rsp2;
  uint64_t _resd2;
  uint64_t ist1;
  uint64_t ist2;
  uint64_t ist3;
  uint64_t ist4;
  uint64_t ist5;
  uint64_t ist6;
  uint64_t ist7;
  uint64_t _resd3;
  uint16_t _resd4;
  uint16_t io_map_base;
  uint8_t tss_io_bitmap[8192];
} PACKED;

_Static_assert(sizeof(struct tss_struct) == 104+8192, "tss_struct should be 8296 bytes");

union gdt_entry {
  struct {
    uint16_t limit1;
    uint16_t base1;
    uint8_t base2;
    uint8_t access_flags;
    uint8_t limit2 :4;
    uint8_t flags2 :4;
    uint8_t base3;
  };

  uint64_t value;
};

typedef union gdt_entry gdt_entry_t;

_Static_assert(sizeof(gdt_entry_t) == 8, "gdt_entry_t should be 8 bytes");

union tss64_descriptor {
  struct {
    uint16_t limit1;
    uint16_t base1;
    uint8_t base2;
    uint8_t access_flags;
    uint8_t limit2 :4;
    uint8_t flags2 :4;
    uint8_t base3;
    uint32_t base4;
    uint32_t _resd;
  };

  struct {
    uint64_t values[2];
  };
};

typedef union tss64_descriptor tss64_entry_t;

_Static_assert(sizeof(tss64_entry_t) == 16, "tss64_entry_t should be 16 bytes");

struct pseudo_descriptor {
  uint16_t limit;
  uint64_t base;
} PACKED;

union idt_entry {
  struct {
    uint16_t offset_lower;
    uint16_t code_selector;
    uint8_t ist; // 0-7
    uint8_t flags; // type (bits 0-3), dpl (bits 5,6), present (bit 7)
    uint16_t offset_mid;
    uint32_t offset_upper;
    uint32_t _resd;
  };

  uint64_t value;
};

typedef union idt_entry idt_entry_t;

_Static_assert(sizeof(idt_entry_t) == 16, "idt_entry_t should be 16 bytes");

extern void atomic_inc(volatile void*);
extern void atomic_dec(volatile void*);
extern long int test_and_set(volatile void*, volatile long int);

static inline void set_cr0(uint64_t new_cr0) {
  __asm__("mov %0, %%cr0" :: "r"(new_cr0));
}

static inline void set_cr3(uint64_t new_cr3) {
  __asm__("mov %0, %%cr3" :: "r"(new_cr3) : "memory");
}

static inline void set_cr4(uint64_t new_cr4) {
  __asm__("mov %0, %%cr4" :: "r"(new_cr4));
}

static inline void set_cr8(uint64_t new_cr8) {
  __asm__("mov %0, %%cr8" :: "r"(new_cr8));
}

static inline void set_rflags(uint64_t new_rflags) {
  __asm__("push %0\n"
	  "popfq\n" :: "r"(new_rflags) : "cc");
}

static inline uint64_t get_cr0(void) {
  uint64_t cr0;

  __asm__("mov %%cr0, %0" : "=r"(cr0));
  return cr0;
}

static inline uint64_t get_cr2(void) {
  uint64_t cr2;

  __asm__("mov %%cr2, %0" : "=r"(cr2));
  return cr2;
}

static inline uint64_t get_cr3(void) {
  uint64_t cr3;

  __asm__("mov %%cr3, %0" : "=r"(cr3));
  return cr3;
}

static inline uint64_t get_cr4(void) {
  uint64_t cr4;

  __asm__("mov %%cr4, %0" : "=r"(cr4));
  return cr4;
}

static inline uint64_t get_cr8(void) {
  uint64_t cr8;

  __asm__("mov %%cr8, %0" : "=r"(cr8));
  return cr8;
}

static inline uint64_t get_rflags(void) {
  uint64_t rflags;

  __asm__("pushfq\n"
	  "pop %0\n" : "=r"(rflags));

  return rflags;
}

static inline uint16_t get_ds(void) {
  uint16_t ds;

  __asm__("mov %%ds, %0" : "=r"(ds));

  return ds;
}

static inline uint16_t get_es(void) {
  uint16_t es;

  __asm__("mov %%es, %0" : "=r"(es));

  return es;
}

static inline uint16_t get_fs(void) {
  uint16_t fs;

  __asm__("mov %%fs, %0" : "=r"(fs));

  return fs;
}

static inline uint16_t get_gs(void) {
  uint16_t gs;

  __asm__("mov %%gs, %0" : "=r"(gs));

  return gs;
}

static inline uint16_t get_ss(void) {
  uint16_t ss;

  __asm__("mov %%ss, %0" : "=r"(ss));

  return ss;
}

static inline void set_cs(uint16_t cs) {
  __asm__("pushq %0\n"
	  "pushq $1f\n"
	  "retfq\n"
	  "1:\n" :: "r"(cs) : "memory");
}

static inline void set_ds(uint16_t ds) {
  __asm__("mov %0, %%ds" :: "r"(ds) : "memory");
}

static inline void set_es(uint16_t es) {
  __asm__("mov %0, %%es" :: "r"(es) : "memory");
}

static inline void set_fs(uint16_t fs) {
  __asm__("mov %0, %%fs" :: "r"(fs));
}

static inline void set_gs(uint16_t gs) {
  __asm__("mov %0, %%gs" :: "r"(gs));
}

static inline void set_ss(uint16_t ss) {
  __asm__("mov %0, %%ss" :: "r"(ss) : "memory");
}

static inline bool is_xsave_supported(void) {
  return IS_FLAG_SET(get_cr4(), CR4_OSXSAVE);
}

static inline bool is_int_enabled(void) {
  return IS_FLAG_SET(get_rflags(), RFLAGS_IF);
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

extern void load_gdt(void);

#define EXT_PTR(var)    (const void * const)( &var )

extern const void *const kcode;
extern const void *const kdata;
extern const void *const kbss;
extern const void *const kend;
extern const void *const ktcb_start;
extern const void *const ktcb_end;
extern const void *const kphys_start;
extern const void *const kvirt_start;
extern const void *const kdcode;
extern const void *const kddata;
extern const void *const kdend;
extern unsigned long int ksize;
extern unsigned long int ktcb_table_size;
extern const void *const kvirt_low_mem_start;

extern const unsigned int kcode_sel;
extern const unsigned int kdata_sel;
extern const unsigned int ucode_sel;
extern const unsigned int udata_sel;
extern const unsigned int ktss_sel;

//extern unsigned int tssEsp0;
extern const unsigned int kVirtToPhys;
extern const unsigned int kPhysToVirt;
extern const unsigned int VPhysMemStart;

extern struct tss_struct tss;

#define SAVE_STATE \
__asm__ ( \
          "push %%rax\n" \
          "push %%rbx\n" \
          "push %%rcx\n" \
          "push %%rdx\n" \
          "push %%rbp\n" \
          "push %%rsi\n" \
          "push %%rdi\n" \
          "push %%r8\n" \
          "push %%r9\n" \
          "push %%r10\n" \
          "push %%r11\n" \
          "push %%r12\n" \
          "push %%r13\n" \
          "push %%r14\n" \
		  "push %%r15\n" \
          "mov %%ds, %%ax\n" \
          "shl $16, %%rax\n" \
          "mov %%es, %%ax\n" \
          "shl $16, %%rax\n" \
          "mov %%fs, %%ax\n" \
          "shl $16, %%rax\n" \
          "mov %%gs, %%ax\n" \
          "push %%rax\n" \
          "lea tss, %%rax\n" \
          "lea 4(%%rax), %%rax\n" \
          "pushq (%%rax)\n" \
          "cmpq $0, (%%rax)\n" /* Is tss.esp0 NULL? (because exception occurred during init()) */ \
          "je 1f\n" \
          "mov %%rsp, (%%rax)\n" \
          "mov %%ss, %%cx\n" \
          "cmpw 144(%%rsp), %%cx\n" /* Privilege Change? */ \
          "je 2f\n" \
          "lea kernel_stack_top, %%rcx\n" \
          "mov %%rcx, (%%rax)\n" /* (user-> kernel switch) Set tss kernel stack ptr as top of kernel stack. */ \
          "jmp 1f\n" \
          "2:\n" /* No privilege change (kernel->kernel switch) */\
          "mov %%rsp, (%%rax)\n" \
          "1:\n" \
          ::: "rax", "rcx", "memory", "cc" \
)

#define SAVE_ERR_STATE \
__asm__ ( \
          "push %%rbx\n" \
          "mov 8(%%rsp), %%rbx\n" /* Save error code to rbx */ \
          "mov %%rax, 8(%%rsp)\n" /* Put rbx where the error code used to be. */ \
          "push %%rcx\n" \
          "push %%rdx\n" \
          "push %%rbp\n" \
          "push %%rsi\n" \
          "push %%rdi\n" \
          "push %%r8\n" \
          "push %%r9\n" \
          "push %%r10\n" \
          "push %%r11\n" \
          "push %%r12\n" \
          "push %%r13\n" \
          "push %%r14\n" \
		  "push %%r15\n" \
          "mov %%ds, %%ax\n" \
          "shl $16, %%rax\n" \
          "mov %%es, %%ax\n" \
          "shl $16, %%rax\n" \
          "mov %%fs, %%ax\n" \
          "shl $16, %%rax\n" \
          "mov %%gs, %%ax\n" \
          "push %%rax\n" \
          "lea tss, %%rax\n" \
          "lea 4(%%rax), %%rax\n" \
          "pushq (%%rax)\n" \
          "cmpq $0, (%%rax)\n" /* Is tss.esp0 NULL? (because exception occurred during init()) */ \
          "je 1f\n" \
          "mov %%rsp, (%%rax)\n" \
          "mov %%ss, %%cx\n" \
          "cmpw 144(%%rsp), %%cx\n" /* Privilege Change? */ \
          "je 2f\n" \
          "lea kernel_stack_top, %%rcx\n" \
          "mov %%rcx, (%%rax)\n" /* (user-> kernel switch) Set tss kernel stack ptr as top of kernel stack. */ \
          "jmp 1f\n" \
          "2:\n" /* No privilege change (kernel->kernel switch) */\
          "mov %%rsp, (%%rax)\n" \
          "1:\n" \
          "push %%rbx\n" \
          ::: "rax", "rbx", "rcx", "memory", "cc" \
)

#define RESTORE_STATE \
__asm__( \
         "lea tss, %%rax\n" /* Restore old stack pointer from tss.esp0 */ \
         "mov 4(%%rax), %%rsp\n" \
         "pop %%rcx\n" \
         "mov %%rcx, 4(%%rax)\n" \
         "pop %%rax\n" \
         "mov %%ax, %%gs\n" \
         "shr $16, %%rax\n" \
         "mov %%ax, %%fs\n" \
         "shr $16, %%rax\n" \
         "mov %%ax, %%es\n" \
         "shr $16, %%rax\n" \
         "mov %%ax, %%ds\n" \
		 "pop %%r15\n" \
         "pop %%r14\n" \
         "pop %%r13\n" \
         "pop %%r12\n" \
         "pop %%r11\n" \
         "pop %%r10\n" \
         "pop %%r9\n" \
         "pop %%r8\n" \
         "pop %%rdi\n" \
         "pop %%rsi\n" \
         "pop %%rbp\n" \
         "pop %%rdx\n" \
         "pop %%rcx\n" \
         "pop %%rbx\n" \
         "pop %%rax\n" \
         "iretq\n" \
         ::: "rax", "rbx", "rcx", "rdx", "rsi", "rdi", \
		 "r8", "r9", "r10", "r11", "r12", "r13", "r14", \
		 "r15", "memory", "cc" \
)

#endif /* KERNEL_LOWLEVEL_H */
