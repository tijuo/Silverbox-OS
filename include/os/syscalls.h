#ifndef OS_SYSCALLS
#define OS_SYSCALLS

#include <types.h>
#include <oslib.h>

#define ESYS_OK          0
#define ESYS_ARG        -1
#define ESYS_FAIL       -2
#define ESYS_PERM       -3
#define ESYS_BADCALL    -4
#define ESYS_NOTIMPL    -5
#define ESYS_NOTREADY   -6
#define ESYS_INT        -7

#ifdef __cplusplus
extern "C" {
#else
#endif /* __cplusplus */

#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNU__MINOR__ >= 9)

#define SYSCALL1(syscallName, type1, arg1) \
static inline long int sys_##syscallName(type1 arg1) { \
  long int ret_val; \
  long int dummy; \
  asm volatile( \
    "pushq %%r11\n" \
    "pushq %%rcx\n" \
    "syscall\n" \
    "popq %%rcx\n" \
    "popq %%r11\n" \
      : "=a"(ret_val), "=D"(dummy) \
      : "a"(SYS_##syscallName), "D"(arg1) \
      : "memory"); \
  return ret_val; \
}

// Dummy variable has to be used because input-only arguments can't be modified

#define SYSCALL2(syscallName, type1, arg1, type2, arg2) \
static inline long int sys_##syscallName(type1 arg1, type2 arg2) { \
  long int ret_val; \
  long int dummy; \
  long int dummy2; \
  asm volatile("pushq %%r11\n" \
        "pushq %%rcx\n" \
        "syscall\n" \
        "popq %%rcx\n" \
        "popq %%r11\n" \
          : "=a"(ret_val), "=D"(dummy), "=S"(dummy2) \
          : "a"(SYS_##syscallName), \
            "D"(arg1), "S"(arg2) \
          : "memory"); \
  return ret_val; \
}

#define SYSCALL3(syscallName, type1, arg1, type2, arg2, type3, arg3) \
static inline long int sys_##syscallName(type1 arg1, type2 arg2, type3 arg3) { \
  long int ret_val; \
  long int dummy; \
  long int dummy2; \
  long int dummy3; \
  asm volatile("pushq %%r11\n" \
        "pushq %%rcx\n" \
        "syscall\n" \
        "popq %%rcx\n" \
        "popq %%r11\n" \
          : "=a"(ret_val), "=D"(dummy), "=S"(dummy2), "=d"(dummy3) \
          : "a"(SYS_##syscallName), \
            "D"(arg1), "S"(arg2), "d"(arg3) \
          : "memory"); \
  return ret_val; \
}

#define SYSCALL4(syscallName, type1, arg1, type2, arg2, type3, arg3, type4, arg4) \
static inline long int sys_##syscallName(type1 arg1, type2 arg2, type3 arg3, type4 arg4) { \
  long int ret_val; \
  long int dummy; \
  long int dummy2; \
  long int dummy3; \
  long int dummy4; \
  asm volatile("pushq %%r11\n" \
        "pushq %%rcx\n" \
        "syscall\n" \
        "popq %%rcx\n" \
        "popq %%r11\n" \
          : "=a"(ret_val), "=D"(dummy), "=S"(dummy2), "=d"(dummy3), "=b"(dummy4) \
          : "a"(SYS_##syscallName), \
            "D"(arg1), "S"(arg2), "d"(arg3), "b"(arg4) \
          : "memory"); \
  return ret_val; \
}

#else
#error "GCC 4.9 or greater is required."
#endif /* __GNUC__ > 4 || (__GNUC__ == 4 && __GNU__MINOR__ >= 9) */

#ifdef __cplusplus
};
#else
#endif /* __cplusplus */
#endif /* OS_SYSCALLS */
