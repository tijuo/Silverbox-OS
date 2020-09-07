#ifndef UTIL_H
#define UTIL_H

#ifdef __GNUC__
#define __PACKED__  __attribute__((packed))
#define __ALIGNED__(x) __attribute__((aligned (x)))
#define HOT(x)  x __attribute__((hot))
#define COLD(x) x __attribute__((cold))
#define PURE(x) x __attribute__((pure))

#define likely(x)      __builtin_expect(!!(x), 1)
#define unlikely(x)    __builtin_expect(!!(x), 0)
#else
#define __PACKED__
#define __ALIGNED__(x)
#define HOT(x)  x
#define COLD(x) x
#define PURE(x) x

#define likely(x)     (!!(x))
#define unlikely(x)   (!!(x))
#endif /* __GNUC__ */

#endif /* UTIL_H */
