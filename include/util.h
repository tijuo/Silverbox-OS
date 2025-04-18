#ifndef UTIL_H
#define UTIL_H

#ifdef __GNUC__
#define PACKED  __attribute__((packed))
#define ALIGN_AS(x) _Alignas(x)
#define HOT  __attribute__((hot))
#define COLD __attribute__((cold))
#define PURE __attribute__((pure))
#define CONST __attribute__((const))
#define NAKED __attribute__((naked))
#define NORETURN _Noreturn
#define SECTION(x) __attribute__((section(x)))
#define UNUSED __attribute__((unused))
#define likely(x)      __builtin_expect(!!(x), 1)
#define unlikely(x)    __builtin_expect(!!(x), 0)
#define NON_NULL_PARAMS __attribute__((nonnull))
#define NON_NULL_PARAM(x) __attribute__((nonnull (x)))
#define RETURNS_NON_NULL __attribute__((returns_nonnull))
#define WARN_UNUSED __attribute__((warn_unused_result))
#else
#define PACKED
#define ALIGN_AS(x) _Alignas(x)
#define HOT
#define COLD
#define PURE
#define CONST
#define NAKED
#define NORETURN  _Noreturn
#define SECTION(x)
#define likely(x)     (!!(x))
#define unlikely(x)   (!!(x))
#define UNUSED
#define NON_NULL_PARAMS
#define NON_NULL_PARAM(x)
#define RETURNS_NON_NULL
#define WARN_UNUSED
#endif /* __GNUC__ */

#ifdef ECLIPSE_GCC
#define _Noreturn __attribute__((noreturn))
#endif /* ECLIPSE_GCC */

#endif /* UTIL_H */
