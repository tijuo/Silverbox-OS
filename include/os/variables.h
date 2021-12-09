#ifndef KERNEL_VARIABLES

#define KERNEL_VARIABLES

#include "../type.h"

#define TICKS_PER_SEC		1024

#define CLOCK_TICKS		0xC4000     // 64-bit (number of milliseconds since 1970)

typedef u64	clktick_t;

#endif /* KERNEL_VARIABLES */
