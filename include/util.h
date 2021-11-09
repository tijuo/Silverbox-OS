#ifndef UTIL_H
#define UTIL_H

#ifdef __GNUC__
#define PACKED  __attribute__((packed))
#define ALIGNED(x) __attribute__((aligned (x)))
#define HOT  __attribute__((hot))
#define COLD __attribute__((cold))
#define PURE __attribute__((pure))
#define CONST __attribute__((const))
#define NAKED __attribute__((naked))
#define NORETURN __attribute__((noreturn))
#define SECTION(x) __attribute__((section(x)))
#define UNUSED __attribute__((unused))
#define likely(x)      __builtin_expect(!!(x), 1)
#define unlikely(x)    __builtin_expect(!!(x), 0)
#else
#define PACKED
#define ALIGNED(x)
#define HOT
#define COLD
#define PURE
#define CONST
#define NAKED
#define NORETURN
#define SECTION(x)
#define likely(x)     (!!(x))
#define unlikely(x)   (!!(x))
#define UNUSED
#endif /* __GNUC__ */

#endif /* UTIL_H */
