#ifndef KERNEL_LOCK_H
#define KERNEL_LOCK_H

#include <stdatomic.h>

#define LOCKED_DECL(type, name) volatile type locked_##name
#define LOCKED_ARRAY_DECL(type, name, size) volatile type locked_##name[size]

#define LOCKED_COUNT(type, name, count) volatile type locked_##name;\
volatile atomic_int lock_##name = (count);

#define LOCKED_WITH_COUNT(type, name, value, count) volatile type locked_##name = (value);\
volatile atomic_int lock_##name = (count);

#define LOCKED(type, name) LOCKED_DECL(type, name);\
volatile atomic_bool lock_##name = false;

#define LOCKED_ARRAY(type, name, size) LOCKED_ARRAY_DECL(type, name, size);\
volatile atomic_bool lock_##name = false;

#define LOCKED_WITH(type, name, value) LOCKED_DECL(type, name) = (value);\
volatile atomic_bool lock_##name = false;

#define LOCK_VAL(name)          (locked_##name)
#define LOCK_SET(name, value)   do { locked_##name = (value); } while(0)

#define SPINLOCK_ACQUIRE(name)     while(lock_##name) { __asm__("pause\n"); }
#define SPINLOCK_RELEASE(name)     do { lock_##name = false; } while(0)

#endif /* KERNEL_LOCK_H */