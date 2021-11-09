#ifndef TYPES_H
#define TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include <util.h>

typedef uint8_t u8;
typedef int8_t s8;

typedef uint16_t u16;
typedef int16_t s16;

typedef uint32_t u32;
typedef int32_t s32;

typedef uint64_t u64;
typedef int64_t s64;

typedef uintptr_t addr_t;
typedef uint32_t paddr_t;

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

#define NULL_TID  					((tid_t)0)
#define NULL_PID  					((pid_t)0)
#define NULL_PADDR					((paddr_t)0xFFFFFFFFu)
#define CURRENT_ROOT_PMAP   NULL_PADDR

#ifndef __cplusplus
/* typedef char _Bool; */

#endif /* __cplusplus */

#ifndef NULL
  #define NULL 0
#endif

#ifndef UNUSED_PARAM
  #ifdef __GNUC__
    #define UNUSED_PARAM	__attribute__ ((unused))
  #else
    #define UNUSED_PARAM
  #endif  /* __GNUC__ */
#endif /* UNUSED_PARAM */

#ifdef __cplusplus
}
#endif

#endif
