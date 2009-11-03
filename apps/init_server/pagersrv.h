#ifndef PAGERSRV_H
#define PAGERSRV_H

#include <types.h>

void *allocEnd;
unsigned int availBytes;

int sysID;

struct BootInfo
{
  unsigned short num_mods;
  unsigned short num_mem_areas;
} __PACKED__;

struct MemoryArea
{
  unsigned long base;
  unsigned long length;
} __PACKED__;

struct BootModule
{
  unsigned long mod_start;
  unsigned long mod_end;
} __PACKED__;

char *server_name;
struct BootInfo *boot_info;
struct MemoryArea *memory_areas;
struct BootModule *boot_modules;
unsigned int *page_dir;
unsigned int total_pages;

void mapVirt( void *virt, int pages );
void mapPage( void );

#endif /* PAGERSRV_H */
