#include <kernel/mm.h>
#include <kernel/paging.h>
#include <kernel/error.h>
#include <kernel/debug.h>
#include <oslib.h>
#include <kernel/lowlevel.h>
#include <kernel/interrupt.h>
#include <stdalign.h>

bool tempMapped = false;

gdt_entry_t kernelGDT[8] = {
  // Null Descriptor (0x00)
  {
    .value = 0
  },

  // Kernel Code Descriptor (0x08)
  {
    {
      .limit1 = 0xFFFFu,
      .base1 = 0,
      .base2 = 0,
      .accessFlags = GDT_READ | GDT_NONSYS | GDT_NONCONF | GDT_CODE | GDT_DPL0
                     | GDT_PRESENT,
      .limit2 = 0xFu,
      .flags2 = GDT_PAGE_GRAN | GDT_BIG,
      .base3 = 0
    }
  },

  // Kernel Data Descriptor (0x10)
  {
    {
      .limit1 = 0xFFFFu,
      .base1 = 0,
      .base2 = 0,
      .accessFlags = GDT_RDWR | GDT_NONSYS | GDT_EXPUP | GDT_DATA | GDT_DPL0
                     | GDT_PRESENT,
      .limit2 = 0xFu,
      .flags2 = GDT_PAGE_GRAN | GDT_BIG,
      .base3 = 0
    }
  },

  // User Code Descriptor (0x1B)
  {
    {
      .limit1 = 0xFFFFu,
      .base1 = 0,
      .base2 = 0,
      .accessFlags = GDT_READ | GDT_NONSYS | GDT_NONCONF | GDT_CODE | GDT_DPL3
                     | GDT_PRESENT,
      .limit2 = 0xFu,
      .flags2 = GDT_PAGE_GRAN | GDT_BIG,
      .base3 = 0
    }
  },

  // User Data Descriptor (0x23)
  {
    {
      .limit1 = 0xFFFFu,
      .base1 = 0,
      .base2 = 0,
      .accessFlags = GDT_RDWR | GDT_NONSYS | GDT_EXPUP | GDT_DATA | GDT_DPL3
                     | GDT_PRESENT,
      .limit2 = 0xFu,
      .flags2 = GDT_PAGE_GRAN | GDT_BIG,
      .base3 = 0
    }
  },

  // TSS Descriptor (0x28)
  {
    {
      .limit1 = 0,
      .base1 = 0,
      .base2 = 0,
      .accessFlags = GDT_SYS | GDT_TSS | GDT_DPL0 | GDT_PRESENT,
      .limit2 = 0,
      .flags2 = GDT_BYTE_GRAN,
      .base3 = 0
    }
  }
};

struct IdtEntry kernelIDT[NUM_EXCEPTIONS + NUM_IRQS];

alignas(PAGE_SIZE) struct TSS_Struct tss SECTION(".tss");

NON_NULL_PARAMS RETURNS_NON_NULL
void* memset(void *ptr, int value, size_t len)
{
  int dummy;

  if((unsigned char)value)
    __asm__ __volatile__("rep stosb\n"
                      : "=c"(dummy)
                      : "a"((unsigned char)value), "c"(len), "D"(ptr)
                      : "memory", "cc");
  else {
    char *p = (char*)ptr;

    while(len && ((uintptr_t)p % 4)) {
      *p++ = 0;
      len--;
    }

    if(len) {
      if(!(unsigned char)value)
        __asm__ __volatile__("rep stosl\n"
                          : "=c"(dummy)
                          : "a"(0), "c"(len / 4), "D"(p)
                          : "memory", "cc");

      p += (len & ~0x03u);
      len -= (len & ~0x03u);
    }

    while(len) {
      *p++ = 0;
      len--;
    }
  }

  return ptr;
}

// XXX: Assumes that the memory regions don't overlap

NON_NULL_PARAMS RETURNS_NON_NULL
void* memcpy(void *restrict dest, const void *restrict src, size_t num)
{
  char *d = (char*)dest;
  const char *s = (const char*)src;

  while(num) {
    *d = *s;
    d++;
    s++;
    num--;
  }

  return dest;
}
