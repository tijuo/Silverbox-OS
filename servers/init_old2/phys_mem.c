#include "phys_mem.h"
#include <os/syscalls.h>
#include <oslib.h>
#include <types.h>
#include <string.h>
#include "phys_mem.h"

#define PAGE_SIZE       4096

char* phys_map = (char*)0x200000;
addr_t* free_page_stack = (addr_t*)0xBF800000, * free_page_stack_top;
static int access_phys(addr_t phys, void* buffer, size_t len, bool read_phys);

addr_t alloc_phys_frame(void)
{
    return ((free_page_stack == free_page_stack_top) ? NULL : *--free_page_stack_top);
}

void free_phys_frame(addr_t page)
{
    *free_page_stack_top++ = page;
}

/**
  Writes data to an address in physical memory.

  @param phys The starting physical address to which data will be copied.
  @param buffer The starting address from which data will be copied.
  @param bytes The number of bytes to be written.
  @return 0 on success. -1 if phys is NULL.
*/

int poke(addr_t phys, void* buffer, size_t bytes)
{
    return access_phys(phys, buffer, bytes, false);
}

/**
  Reads data from an address in physical memory.

  @param phys The starting physical address from which data will be copied.
  @param buffer The starting address to which data will be copied.
  @param bytes The number of bytes to be read.
  @return 0 on success. -1 if phys is NULL.
*/

int peek(addr_t phys, void* buffer, size_t bytes)
{
    return access_phys(phys, buffer, bytes, true);
}

static int access_phys(addr_t phys, void* buffer, size_t len, bool read_phys)
{
    size_t offset = (size_t)(phys & (PAGE_SIZE - 1));
    size_t num_pages = 1 + (((phys + len) & ~(PAGE_SIZE - 1)) - (phys & ~(PAGE_SIZE - 1))) / PAGE_SIZE;

    if(len > 0xE00000) // Unable to handle larger sizes due to location of phys_map
        return -1;

    pbase_t frame = (pbase_t)(phys >> 12);

    if(sys_map(NULL, (void*)phys_map, &frame, num_pages, PM_READ_WRITE) != (int)num_pages)
        return -1;

    if(read_phys) {
        memcpy(buffer, (void*)(phys_map + offset), len);
    } else {
        memcpy((void*)(phys_map + offset), buffer, len);
    }

    sys_unmap(NULL, (void*)phys_map, num_pages);

    return 0;
}
