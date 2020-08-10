#include <os/multiboot.h>
#include <os/syscalls.h>
#include <oslib.h>
#include <types.h>
#include <string.h>
#include <stdlib.h>
#include "phys_mem.h"
#include "addr_space.h"
#include "pager.h"
#include <os/elf.h>
#include <os/services.h>

char *itoa(int, char *, int, int);
void printC(char);
void printN(const char *, int);
void print(const char *);
void printInt(int);
void printHex(int);
int initPageStack(multiboot_info_t *info, addr_t lastFreeKernelPage);
static int loadElfExec(module_t *module);
static void handleMessage(msg_t *msg);

char *_digits = "0123456789abcdefghijklmnopqrntuvwxyz";

char *itoa(int value, char *str, int base, int unSigned)
{
  unsigned int num = (unsigned int)value;
  size_t str_end=0;
  int negative = base == 10 && !unSigned && (value & 0x80000000);

  if(negative)
    num = ~num + 1;

  if( !str )
    return NULL;
  else if( base >= 2 && base <= 36 )
  {
    do
    {
      unsigned int quot = base * (num / base);

      str[str_end++] = _digits[num - quot];
      num = quot / base;
    } while( num );

    if(negative)
      str[str_end++] = '-';
  }

  str[str_end] = '\0';

  if( str_end )
  {
    str_end--;

    for( size_t i=0; i < str_end; i++, str_end-- )
    {
      char tmp = str[i];
      str[i] = str[str_end];
      str[str_end] = tmp;
    }
  }
  return str;
}

void printC( char c )
{
  char volatile *vidmem = (char volatile *)(0xB8000 + 160 * 4);
  static int i=0;

  if( c == '\n' )
    i += 160 - (i % 160);
  else
  {
    vidmem[i++] = c;
    vidmem[i++] = 7;
  }
}

void printN( const char *str, int n )
{
  if(!str)
    return;

  for( int i=0; i < n; i++, str++ )
    printC(*str);
}

void print( const char *str )
{
  if(!str)
    return;

  for(; *str; str++ )
    printC(*str);
}

void printInt( int n )
{
  char s[11];
  print(itoa(n, s, 10, 0));
}

void printHex( int n )
{
  char s[9];
  print(itoa(n, s, 16, 1));
}

int initPageStack(multiboot_info_t *info, addr_t lastFreeKernelPage)
{
  memory_map_t mmap;
  size_t freePageCount=0;
  multiboot_info_t multibootStruct;

  freePageStackTop = freePageStack;

  // Read the multiboot info structure from physical memory

  if(peek((addr_t)info, &multibootStruct, sizeof(multibootStruct)) != 0)
  {
    print("Failed to read multiboot struct.\n");
    return 1;
  }

  /* Count the total number of pages and the pages we'll need to map the stack.
     Then map the stack with those pages. Finally, add the remaining pages to
     the free stack. */

  for(int pass=0; pass < 2; pass++)
  {
    addr_t freePage = lastFreeKernelPage + PAGE_SIZE;
    size_t pagesNeeded, pagesLeft;
    addr_t ptr;

    if(pass == 1)
    {
      pagesNeeded = (freePageCount * sizeof(addr_t)) / PAGE_SIZE;

      if((freePageCount * sizeof(addr_t)) & (PAGE_SIZE-1))
        pagesNeeded++;

      pagesLeft = freePageCount - pagesNeeded;
      ptr = (addr_t)freePageStack;
    }

    for(unsigned int offset=0; offset < multibootStruct.mmap_length; offset += mmap.size+sizeof(mmap.size))
    {
      if(peek((addr_t)(multibootStruct.mmap_addr+offset), &mmap, sizeof(memory_map_t)) != 0)
      {
        print("Failed to read memory map");
        return 1;
      }

      if(mmap.type == MBI_TYPE_AVAIL)
      {
        u64 mmapLen = mmap.length_high;
        mmapLen = mmapLen << 32;
        mmapLen |= mmap.length_low;

        u64 mmapBase = mmap.base_addr_high;
        mmapBase = mmapBase << 32;
        mmapBase |= mmap.base_addr_low;

        if(pass == 0)
          for( ; freePage >= mmapBase && freePage < mmapBase + mmapLen; freePage += PAGE_SIZE, freePageCount++);
        else
        {
          printInt(pagesNeeded);
          print(" pages needed.\n");
          printInt(pagesLeft);
          print(" pages left.\n");

          if(sys_map(NULL, ptr, (pframe_t)(freePage >> 12), pagesNeeded, PM_READ_WRITE) != ESYS_OK)
          {
            print("Unable to allocate memory for free page stack.\n");
            return 1;
          }

          freePage += PAGE_SIZE * pagesNeeded;
          ptr += PAGE_SIZE * pagesNeeded;
          pagesNeeded = 0;

          for(; pagesLeft; pagesLeft--, freePage += PAGE_SIZE)
            *freePageStackTop++ = freePage;
        }
      }
    }
  }

  return 0;
}

static void handleMessage(msg_t *msg)
{
  if(msg->sender == KERNEL_TID)
  {
    switch(msg->subject)
    {
      case EXCEPTION_MSG:
        print("Exception ");
        printInt(msg->data.i32[0]);
        print(" for TID ");
        printInt(msg->data.i32[3]);
        print(". Error code: 0x");
        printHex(msg->data.i32[1]);

        if(msg->data.i32[0] == 14)
        {
          print(" Fault address: 0x");
          printHex(msg->data.i32[2]);
          print("\n");
        }
        break;
      case EXIT_MSG:
        print("TID ");
        printInt(msg->data.i32[3]);
        print(" exited with status code: ");
        printInt(msg->data.i32[1]);
        break;
      default:
        break;
    }
  }
  else
  {
    switch(msg->data.c8[0])
    {
      case MAP_REGION:
        break;
      case UNMAP_REGION:
        break;
      case CREATE_PORT:
        break;
      case LISTEN_PORT:
        break;
      case DESTROY_PORT:
        break;
      case SEND_MESSAGE:
        break;
      case RECEIVE_MESSAGE:
        break;
      default:
        break;
    }
  }
}

static int loadElfExec(module_t *module)
{
  unsigned phtab_count;
  elf_header_t *image;
  elf_pheader_t *pheader;
  tid_t tid;
  int fail=0;
  addr_t stackTop = (addr_t)0xC0000000;
  size_t modSize = module->mod_end-module->mod_start;

  if(!module|| !(image=malloc(modSize)))
    return -1;

  for(size_t offset=0; offset < modSize; offset += 0x200000u)
  {
    if(peek(module->mod_start+offset, (void *)((unsigned int)image + offset),
            MIN(modSize-offset, 0x200000u)) != ESYS_OK)
    {
      free(image);
      return -1;
    }
  }

  if(!isValidElfExe(image))
  {
    print("Not a valid ELF executable.\n");
    return -1;
  }

  u32 pmap = allocPhysPage();
  struct AddrSpace *addrSpace = malloc(sizeof(struct AddrSpace));

  if(!pmap || !addrSpace)
    fail = 1;
  else
  {
    initAddrSpace(addrSpace, (paddr_t)pmap);
    phtab_count = image->phnum;
    pheader = (elf_pheader_t *)((unsigned)image + image->phoff);

    for(unsigned int i = 0; i < phtab_count; i++, pheader++)
    {
      if(fail)
        break;

      if ( pheader->type == PT_LOAD )
      {
        unsigned int memSize   = pheader->memsz;
        unsigned int fileSize  = pheader->filesz;
        unsigned int filePages = (fileSize / PAGE_SIZE) + ((fileSize & (PAGE_SIZE-1)) ? 1 : 0);
        unsigned int memPages  = (memSize / PAGE_SIZE) + ((memSize & (PAGE_SIZE-1)) ? 1 : 0);

        int flags = ((pheader->flags & PF_W) ? 0 : REG_RO) | ((pheader->flags & PF_X) ? 0 : REG_NOEXEC);

        if(fileSize > 0 && mapRegion(addrSpace, pheader->vaddr, pheader->offset + module->mod_start,
                                     filePages, flags) != 0)
        {
          fail = 1;
          break;
        }
        else if(memPages > filePages && mapRegion(addrSpace, pheader->vaddr + filePages*PAGE_SIZE,
                                             module->mod_start + pheader->offset + filePages*PAGE_SIZE,
                                             memPages-filePages, flags | REG_ZERO) != 0)
        {
          fail = 1;
          break;
        }
      }
    }
  }

  if(!fail)
  {
    thread_info_t threadInfo =
    {
      .status = TCB_STATUS_READY
    };

    tid = sys_create_thread((addr_t)image->entry, pmap, stackTop);

    if(tid == NULL_TID || attachTid(addrSpace, tid) != 0 || sys_update_thread(tid, TF_STATUS, &threadInfo) != 0)
      fail = 1;
  }

  if(fail)
  {
    if(tid)
      detachTid(tid);

    if(addrSpace)
    {
      destroyAddrSpace(addrSpace);
      free(addrSpace);
    }

    if(pmap)
      freePhysPage(pmap);

    return -1;
  }

  return 0;
}

int main(multiboot_info_t *info, addr_t lastFreeKernelPage)
{
  print("init server started.\n");

  if(initPageStack(info, lastFreeKernelPage) != 0)
    print("Unable to initialize the free page stack.\n");
  else
    print("Free page stack initialized.\n");

  multiboot_info_t multibootStruct;
  module_t module;

  peek((addr_t)info, &multibootStruct, sizeof multibootStruct);

  for(unsigned i=0; i < multibootStruct.mods_count; i++)
  {
    peek(addr_t)multibootStruct.mods_addr + i*sizeof(module_t), &module, sizeof(module_t));
  }

/*
  while(1)
  {
    msg_t msg =
    {
      .recipient = ANY_SENDER
    };

    if(sys_receive(&msg, 1) >= 0)
    {
      print("Received a message from ");

      if(msg.sender == KERNEL_TID)
        print("the kernel");
      else
      {
        print("TID: ");
        printInt(msg.sender);
      }

      print(".\n");

      handleMessage(&msg);
    }
  }
*/
}
