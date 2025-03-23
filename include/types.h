#ifndef TYPES_H
#define TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <util.h>

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

    typedef uint8_t u8;
    typedef int8_t s8;

    typedef uint16_t u16;
    typedef int16_t s16;

    typedef uint32_t u32;
    typedef int32_t s32;

    typedef uint64_t u64;
    typedef int64_t s64;

    typedef uintptr_t addr_t;

    typedef u8  byte;
    typedef u16 word;
    typedef u32 dword;
    typedef u64 qword;

    typedef u8  uchar;
    typedef uchar uchar8;
    typedef u16 uchar16;
    typedef u16 ushort;
    typedef u16 uint16;
    typedef u32 uint32;
    typedef u32 uint;
    typedef u64 uint64;
    typedef u64 uquad;

    typedef s8 char8;
    typedef s16 char16;
    typedef s16 int16;
    typedef s32 int32;
    typedef s64 int64;
    typedef s64 quad;

    typedef unsigned short int tid_t;
    typedef unsigned short int pid_t;

    typedef unsigned long int pbase_t;
    typedef unsigned long int paddr_t;
    typedef signed long int spaddr_t;

    // 288 bytes

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
        } xmm[8];

        // Even though the area is reserved, the processor won't access the reserved bits in 32-bit mode
        /*
         uint8_t _resd12[176];
         uint8_t available[48];
         */
    } xsave_state_t;

#define NULL_TID  		((tid_t)0)
#define NULL_PID  		((pid_t)0)
#define CURRENT_ROOT_PMAP   	~0ul
#define INVALID_PBASE		~0ul
#define INVALID_PADDR       ~0ul

#ifndef __cplusplus
    /* typedef char _Bool; */

#endif /* __cplusplus */

#ifndef NULL
#define NULL 0
#endif /* NULL */

#ifndef UNUSED_PARAM
#ifdef __GNUC__
#define UNUSED_PARAM	__attribute__ ((unused))
#else
#define UNUSED_PARAM
#endif  /* __GNUC__ */
#endif /* UNUSED_PARAM */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* TYPES_H */
