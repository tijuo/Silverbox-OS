#ifndef TYPES_H
# define TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include <util.h>

typedef unsigned char u8;
typedef signed char s8;

typedef unsigned short u16;
typedef signed short s16;

typedef unsigned int u32;
typedef signed int s32;

typedef unsigned long long u64;
typedef signed long long s64;

typedef u32 addr_t;
typedef u64 paddr_t;

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

#define NULL_TID  ((tid_t)0)
#define NULL_PID  ((pid_t)0)

#ifndef __cplusplus
/* typedef char _Bool; */

#endif /* __cplusplus */

#ifndef NULL
  #define NULL ((void *)0)
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
