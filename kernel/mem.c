#include <kernel/mm.h>
#include <kernel/paging.h>
#include <kernel/error.h>
#include <kernel/debug.h>
#include <oslib.h>
#include <kernel/lowlevel.h>
#include <kernel/interrupt.h>
#include <kernel/memory.h>
#include <stdalign.h>

struct IdtEntry kernel_idt[NUM_EXCEPTIONS + NUM_IRQS];

alignas(PAGE_SIZE) struct tss_struct tss SECTION(".tss");

NON_NULL_PARAMS RETURNS_NON_NULL
void* memset(void *ptr, int value, size_t len)
{
  long int dummy;
  long int dummy2;

  __asm__ __volatile__(
	  "cld\n"
	  "rep stosb\n"
	  : "=c"(dummy), "D"(dummy2)
	  : "a"((unsigned char)value), "c"(len), "D"(ptr)
	  : "memory", "cc");

  return ptr;
}

// XXX: Assumes that the memory regions don't overlap

NON_NULL_PARAMS RETURNS_NON_NULL
void* memcpy(void *restrict dest, const void *restrict src, size_t num)
{
  long int dummy;
  long int dummy2;
  long int dummy3;

  __asm__ __volatile__(
	  "cld\n"
	  "rep movsb\n"
	  : "=c"(dummy), "D"(dummy2), "S"(dummy3)
	  : "c"(len), "S"(src), "D"(dest)
	  : "memory", "cc");

  return dest;
}
