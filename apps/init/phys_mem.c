#include "phys_mem.h"
#include <os/syscalls.h>
#include <oslib.h>
#include <types.h>
#include <string.h>
#include "phys_mem.h"

#define PAGE_SIZE       4096

char *physMap = (char *)0x200000;
addr_t *freePageStack = (addr_t *)0xC00000, *freePageStackTop;
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
  size_t offset, bytes;

  if(len > 0x200000) // XXX: because of the address used for pageMap, we're unable to access more than 2 MB
    return -1;

  for( size_t i=0; len; phys += bytes, i += bytes, len -= bytes )
  {
    offset = (size_t)(phys & (PAGE_SIZE - 1));
    bytes = (len > PAGE_SIZE - offset) ? PAGE_SIZE - offset : len;

    sys_map(NULL, (addr_t)physMap, phys >> 12, 1, PM_READ_WRITE);

    if( readPhys )
      memcpy( (void *)((addr_t)buffer + i), (void *)(physMap + offset), bytes);
    else
      memcpy( (void *)(physMap + offset), (void *)((addr_t)buffer + i), bytes);

    sys_unmap(NULL, (addr_t)physMap, 1);
  }

  return 0;
}
