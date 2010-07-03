#ifndef INITSRV_H
#define INITSRV_H

#include "addr_space.h"
#include "resources.h"
#include <types.h>

void *allocEnd;
unsigned int availBytes;

int sysID;

struct ResourcePool *initsrv_pool;

struct ProgramArgs
{
  char *data;
  int length;
};

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

enum ExeLocation { MEMORY, DISK };

struct ExeSections
{
  unsigned long codeVirt;
  unsigned long codePhys;
  unsigned long codeLength;

  unsigned long dataVirt;
  unsigned long dataPhys;
  unsigned long dataLength;

  unsigned long bssVirt;
  unsigned long bssPhys;
  unsigned long bssLength;

  enum ExeLocation location;
  char *name;
  size_t nameLen;
};

char *server_name;
struct BootInfo *boot_info;
struct MemoryArea *memory_areas;
struct BootModule *boot_modules;
unsigned int *page_dir; // The initial server page directory
unsigned int total_pages;

void mapVirt( void *virt, int pages );
void mapPage( void );

#endif /* INITSRV_H */
