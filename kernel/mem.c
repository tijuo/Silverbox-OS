#include <kernel/mm.h>
#include <kernel/paging.h>
#include <kernel/error.h>
#include <kernel/debug.h>
#include <oslib.h>
#include <kernel/lowlevel.h>
#include <kernel/interrupt.h>
#include <stdalign.h>

addr_t *freePageStack = (addr_t*)PAGE_STACK;
addr_t *freePageStackTop;
bool tempMapped = false;

pte_t kMapAreaPTab[PTE_ENTRY_COUNT] ALIGNED(PAGE_SIZE);
uint8_t kernelStack[PAGE_SIZE] ALIGNED(PAGE_SIZE); // The single kernel stack used by all threads (assumes a uniprocessor system)
uint8_t *kernelStackTop = kernelStack + PAGE_SIZE;

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
  },

  // Bootstrap Code Descriptor (0x30)
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

  // Bootstrap Data Descriptor (0x38)
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
  }
};

struct IdtEntry kernelIDT[NUM_EXCEPTIONS + NUM_IRQS];

alignas(PAGE_SIZE) struct TSS_Struct tss SECTION(".tss");

void clearMemory(void *ptr, size_t len) {
#ifdef DEBUG
  if((size_t)ptr % 4 != 0 || len % 4 != 0) {
    kprintf(
        "clearMemory(): Start address (0x%p) must have 4-byte alignment and length (%d) must be divisible by 4. Using memset() instead...\n",
        ptr, len);
    memset(ptr, 0, len);
    return;
  }
#endif
  __asm__ __volatile__("rep stosl\n" :: "a"(0), "c"(len / 4), "D"(ptr) : "memory");
}

void* memset(void *ptr, int value, size_t num) {
  char *p = (char*)ptr;

  while(num) {
    *p = (char)value;
    p++;
    num--;
  }

  return ptr;
}

// XXX: Assumes that the memory regions don't overlap

void* memcpy(void *dest, const void *src, size_t num) {
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

/** Allocate an available 4 KB physical page frame.

 @return The physical address of a newly allocated page frame. INVALID_PFRAME, on failure.
 **/

addr_t allocPageFrame(void) {
#ifdef DEBUG
  if(!freePageStackTop)
    RET_MSG(INVALID_PFRAME,
            "allocPageFrame(): Free page stack hasn't been initialized yet.");
#endif /* DEBUG */

#ifdef DEBUG
  else
    assert(
        *(freePageStackTop-1) == ALIGN_DOWN(*(freePageStackTop-1), PAGE_SIZE))
#endif /* DEBUG */

  if(freePageStackTop == freePageStack)
    RET_MSG(INVALID_PFRAME,
            "Kernel has no more available physical page frames.");
  else
    return *--freePageStackTop;
}

/** Release a 4 KB page frame.

 @param frame The physical address of the page frame to be released
 **/

void freePageFrame(addr_t frame) {
  assert(frame == ALIGN_DOWN(frame, PAGE_SIZE));
  assert(freePageStackTop);
  assert(frame != INVALID_PFRAME);

#ifdef DEBUG
  if(freePageStackTop)
#endif /* DEBUG */
    *freePageStackTop++ = ALIGN_DOWN(frame, PAGE_SIZE);

#ifdef DEBUG
  else
    PRINT_DEBUG("Free page stack hasn't been initialized yet\n");
#endif /* DEBUG */
}
