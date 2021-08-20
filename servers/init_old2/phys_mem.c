#include "phys_mem.h"
#include <os/syscalls.h>
#include <oslib.h>
#include <types.h>
#include <string.h>
#include "phys_mem.h"

#define PAGE_SIZE       4096

char *physMap = (char *)0x200000;
addr_t *freePageStack = (addr_t *)0xBF800000, *freePageStackTop;
static int accessPhys(addr_t phys, void *buffer, size_t len, bool readPhys);

addr_t allocPhysPage(void)
{
  return ((freePageStack == freePageStackTop) ? NULL : *--freePageStackTop);
}

void freePhysPage(addr_t page)
{
  *freePageStackTop++ = page;
}

/**
  Writes data to an address in physical memory.

  @param phys The starting physical address to which data will be copied.
  @param buffer The starting address from which data will be copied.
  @param bytes The number of bytes to be written.
  @return 0 on success. -1 if phys is NULL.
*/

int poke( addr_t phys, void *buffer, size_t bytes )
{
  return accessPhys( phys, buffer, bytes, false );
}

/**
  Reads data from an address in physical memory.

  @param phys The starting physical address from which data will be copied.
  @param buffer The starting address to which data will be copied.
  @param bytes The number of bytes to be read.
  @return 0 on success. -1 if phys is NULL.
*/

int peek( addr_t phys, void *buffer, size_t bytes )
{
  return accessPhys( phys, buffer, bytes, true );
}

static int accessPhys(addr_t phys, void *buffer, size_t len, bool readPhys)
{
  size_t offset=(size_t)(phys & (PAGE_SIZE - 1));
  size_t numPages = 1 + (((phys+len) & ~(PAGE_SIZE-1)) - (phys & ~(PAGE_SIZE-1))) / PAGE_SIZE;

  if(len > 0xE00000) // Unable to handle larger sizes due to location of physMap
    return -1;

  pframe_t frame = (pframe_t)(phys >> 12);

  if(sys_map(NULL, (void *)physMap, &frame, numPages, PM_READ_WRITE) != (int)numPages)
    return -1;

  if( readPhys )
    memcpy(buffer, (void *)(physMap + offset), len);
  else
    memcpy((void *)(physMap + offset), buffer, len);

  sys_unmap(NULL, (void *)physMap, numPages);

  return 0;
}
