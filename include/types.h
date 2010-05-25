#ifndef TYPES_H
# define TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdbool.h>

typedef unsigned char u8;
typedef signed char s8;

typedef unsigned short u16;
typedef signed short s16;

typedef unsigned long u32;
typedef signed long s32;

typedef unsigned long long u64;
typedef signed long long s64;

typedef void * ptr_t;
typedef unsigned char * addr_t;

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

#ifndef __cplusplus
/* typedef char _Bool; */

#endif /* __cplusplus */

#ifdef __GNUC__
  #define __PACKED__	__attribute__((packed))
  #define HOT(x)	x __attribute__((hot))
  #define COLD(x)	x __attribute__((cold))
  #define PURE(x)	x __attribute__((pure))
#else
  #define __PACKED__
  #define HOT(x)	x
  #define COLD(x)	x
  #define PURE(x)	x
#endif

#ifndef NULL
  #define NULL 0
#endif

#ifdef __cplusplus
}
#endif

#endif
