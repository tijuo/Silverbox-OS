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

#define enable_int()     __asm__ __volatile__("sti" ::: "cc")
#define disable_int()   __asm__ __volatile__("cli" ::: "cc")
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
    } same_priv;

    struct {
        uint32_t eip;
        uint16_t cs;
        uint16_t _resd;
        uint32_t eflags;
        uint16_t ss;
        uint16_t _resd2;
        uint32_t esp;
    } change_priv;
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
    uint16_t trap : 1;
    uint16_t _resd12 : 15;
    uint16_t io_map_base;
    uint8_t tss_io_bitmap[8192];
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
    uint32_t user_esp;
    uint16_t user_ss;
    uint16_t avail2;
} ExecutionState;

union GdtEntry {
    struct {
        uint16_t limit1;
        uint16_t base1;
        uint8_t base2;
        uint8_t access_flags;
        uint8_t limit2 : 4;
        uint8_t flags2 : 4;
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
    uint16_t offset_lower;
    uint16_t selector;
    uint8_t _resd;
    uint8_t gate_type : 4;
    uint8_t is_storage : 1;
    uint8_t dpl : 2;
    uint8_t is_present : 1;
    uint16_t offset_upper;
};

typedef struct IdtEntry idt_entry_t;

_Static_assert(sizeof(idt_entry_t) == 8, "IDTEntry should be 8 bytes");

struct IdtPointer {
    uint16_t limit;
    uint32_t base;
} PACKED;

extern void atomic_inc(volatile void*);
extern void atomic_dec(volatile void*);
extern int test_and_set(volatile void*, volatile int);

static inline void set_cr0(uint32_t new_cr0)
{
    __asm__("mov %0, %%cr0" :: "r"(new_cr0));
}

static inline void set_cr3(uint32_t new_cr3)
{
    __asm__("mov %0, %%cr3" :: "r"(new_cr3) : "memory");
}

static inline void set_cr4(uint32_t new_cr4)
{
    __asm__("mov %0, %%cr4" :: "r"(new_cr4));
}

static inline void set_eflags(uint32_t newEflags)
{
    __asm__("pushl %0\n"
        "popf\n" :: "r"(newEflags) : "cc");
}

static inline uint32_t get_cr0(void)
{
    uint32_t cr0;

    __asm__("mov %%cr0, %0" : "=r"(cr0));
    return cr0;
}

static inline uint32_t get_cr2(void)
{
    uint32_t cr2;

    __asm__("mov %%cr2, %0" : "=r"(cr2));
    return cr2;
}

static inline uint32_t get_cr3(void)
{
    uint32_t cr3;

    __asm__("mov %%cr3, %0" : "=r"(cr3));
    return cr3;
}

static inline uint32_t get_cr4(void)
{
    uint32_t cr4;

    __asm__("mov %%cr4, %0" : "=r"(cr4));
    return cr4;
}

static inline uint32_t get_eflags(void)
{
    uint32_t eflags;

    __asm__("pushf\n"
        "popl %0\n" : "=r"(eflags));

    return eflags;
}

static inline uint16_t get_ds(void)
{
    uint16_t ds;

    __asm__("mov %%ds, %0" : "=r"(ds));

    return ds;
}

static inline uint16_t get_es(void)
{
    uint16_t es;

    __asm__("mov %%es, %0" : "=r"(es));

    return es;
}

static inline uint16_t get_fs(void)
{
    uint16_t fs;

    __asm__("mov %%fs, %0" : "=r"(fs));

    return fs;
}

static inline uint16_t get_gs(void)
{
    uint16_t gs;

    __asm__("mov %%gs, %0" : "=r"(gs));

    return gs;
}

static inline uint16_t get_ss(void)
{
    uint16_t ss;

    __asm__("mov %%ss, %0" : "=r"(ss));

    return ss;
}

static inline void set_cs(uint16_t cs)
{
    __asm__("push %0\n"
        "push $1f\n"
        "retf\n"
        "1:\n" :: "r"(cs) : "memory");
}

static inline void set_ds(uint16_t ds)
{
    __asm__("mov %0, %%ds" :: "r"(ds) : "memory");
}

static inline void set_es(uint16_t es)
{
    __asm__("mov %0, %%es" :: "r"(es) : "memory");
}

static inline void set_fs(uint16_t fs)
{
    __asm__("mov %0, %%fs" :: "r"(fs));
}

static inline void set_gs(uint16_t gs)
{
    __asm__("mov %0, %%gs" :: "r"(gs));
}

static inline void set_ss(uint16_t ss)
{
    __asm__("mov %0, %%ss" :: "r"(ss) : "memory");
}

static inline bool is_xsave_supported(void)
{
    return IS_FLAG_SET(get_cr4(), CR4_OSXSAVE);
}

static inline bool is_int_enabled(void)
{
    return IS_FLAG_SET(get_eflags(), EFLAGS_IF);
}

static inline void wrmsr(uint32_t msr, uint64_t data)
{
    uint32_t eax = (uint32_t)data;
    uint32_t edx = (uint32_t)(data >> 32);

    __asm__ __volatile__("wrmsr" :: "c"(msr), "a"(eax), "d"(edx));
}

static inline uint64_t rdmsr(uint32_t msr)
{
    uint32_t eax;
    uint32_t edx;

    __asm__ __volatile__("rdmsr" : "=a"(eax), "=d"(edx) : "c"(msr));

    return ((uint64_t)edx << 32) | (uint64_t)eax;
}

extern void load_gdt(void);

#define EXT_PTR(var)    (const void * const)( &var )

extern const void* const kcode;
extern const void* const kdata;
extern const void* const kbss;
extern const void* const kend;
extern const void* const ktcb_start;
extern const void* const ktcb_end;
extern const void* const kphys_start;
extern const void* const kvirt_start;
extern const void* const kdcode;
extern const void* const kddata;
extern const void* const kdend;
extern unsigned long int ksize;
extern unsigned long int ktcb_table_size;
extern const void* const kvirt_low_mem_start;

extern const unsigned int kcode_sel;
extern const unsigned int kdata_sel;
extern const unsigned int ucode_sel;
extern const unsigned int udata_sel;
extern const unsigned int ktss_sel;

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
          "lea kernel_stack_top, %%ecx\n" \
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
          "mov %%ss, %%dx\n" \
          "cmpw 54(%%esp), %%dx\n" /* Privilege Change? */ \
          "je 2f\n" \
          "lea kernel_stack_top, %%edx\n" \
          "mov %%edx, (%%eax)\n" /* (user-> kernel switch) Set tss kernel stack ptr as top of kernel stack. */ \
          "jmp 1f\n" \
          "2:\n" /* No privilege change (kernel->kernel switch) */\
          "mov %%esp, (%%eax)\n" \
          "1:\n" \
          "push %%ecx\n" \
          "push %%ecx\n" \
          ::: "eax", "ecx", "edx", "memory", "cc" \
)

#define RESTORE_STATE \
__asm__( \
         "lea tss, %%eax\n" /* Restore old stack pointer from tss.esp0 */ \
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

#endif /* KERNEL_LOWLEVEL_H */
