#include <kernel/mm.h>
#include <kernel/paging.h>
#include <kernel/error.h>
#include <kernel/debug.h>
#include <oslib.h>

addr_t *freePageStack=(addr_t *)PAGE_STACK;
addr_t *freePageStackTop;
bool tempMapped=false;

uint8_t kPageDir[PAGE_SIZE] ALIGNED(PAGE_SIZE);
uint8_t kPageTab[PAGE_SIZE] ALIGNED(PAGE_SIZE);
uint8_t kMapAreaPTab[PAGE_SIZE] ALIGNED(PAGE_SIZE);
uint8_t kernelStack[PAGE_SIZE] ALIGNED(PAGE_SIZE);
uint8_t *kernelStackTop = kernelStack + PAGE_SIZE;

void clearMemory(void *ptr, size_t len)
{
#ifdef DEBUG
  if((size_t)ptr % 4 != 0 || len % 4 != 0)
  {
    kprintf("clearMemory(): Start address (0x%p) must have 4-byte alignment and length (%d) must be divisible by 4. Using memset() instead...\n", ptr, len);
    memset(ptr, 0, len);
    return;
  }
#endif
  asm("xor %%eax, %%eax\n"
      "rep stosl\n" :: "c"(len / 4), "D"(ptr) : "memory");
}

void *memset(void *ptr, int value, size_t num)
{
  char *p = (char *)ptr;

  while(num)
  {
    *p = value;
    p++;
    num--;
  }

  return ptr;
}

// XXX: Assumes that the memory regions don't overlap

void *memcpy(void *dest, const void *src, size_t num)
{
  char *d = (char *)dest;
  const char *s = (const char *)src;

  while(num)
  {
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

addr_t allocPageFrame(void)
{
#ifdef DEBUG
  if(!freePageStackTop)
    RET_MSG(INVALID_PFRAME, "allocPageFrame(): Free page stack hasn't been initialized yet.");
#endif /* DEBUG */

#ifdef DEBUG
  else
    assert(*(freePageStackTop-1) == ALIGN_DOWN(*(freePageStackTop-1), PAGE_SIZE))
#endif /* DEBUG */

    if(freePageStackTop == freePageStack)
      RET_MSG(INVALID_PFRAME, "Kernel has no more available physical page frames.");
    else
      return *--freePageStackTop;
}

/** Release a 4 KB page frame.

    @param frame The physical address of the page frame to be released
 **/

void freePageFrame(addr_t frame)
{
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
