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
#include <os/msg/message.h>
#include <os/msg/init.h>
#include <os/msg/kernel.h>
#include "name/name.h"

#define STACK_TOP		0xC0000000

extern sbhash_t threadNames, deviceNames, fsNames, deviceTable, fsTable;

sbhash_t serverTable; // "tid" -> ServerEntry

void printC(char);
void printN(const char *, int);
void print(const char *);
void printInt(int);
void printHex(int);
int initPageStack(multiboot_info_t *info, addr_t lastFreeKernelPage);
static int loadElfExe(module_t *module);
static void handleMessage(msg_t *msg);
tid_t createNewThread(tid_t desiredTid, addr_t entry, addr_t pageMap, addr_t stackTop);

extern void (*idle)(void);
int startThread(tid_t tid);
int setPriority(tid_t tid, int priority);

struct ServerEntry
{
  tid_t tid;
  int type;
};

void printC( char c )
{
  char volatile *vidmem = (char volatile *)(0xB8000 + 160 * 2);
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
  print(itoa(n, s, 10));
}

void printHex( int n )
{
  char s[9];
  print(itoa(n, s, 16));
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
      {
        struct ExceptionMessage *exMessage = (struct ExceptionMessage *)&msg->data;
        addr_t faultAddr = exMessage->faultAddress;
        tid_t tid = exMessage->who;
        int intNum = exMessage->intNum;
        int errorCode = exMessage->errorCode;
/*
        print("Exception ");
        printInt(intNum);
        print(" for TID ");
        printInt(tid);
        print(". Error code: 0x");
        printHex(errorCode);
*/
        if(intNum == 14)
        {
//          print(" Fault address: 0x");
//          printHex(faultAddr);

          if(!(errorCode & 0x4))
          {

            print(" Illegal access to supervisor memory.\n");
            break;
          }

          struct AddrSpace *aspace=lookupTid(tid);

          if(!aspace)
            print("Unable to find address space for TID "), printInt(tid), print("\n");

          if(findAddress(aspace, faultAddr))
          {
            struct AddrRegion *region = getRegion(aspace, faultAddr);
            page_t *page;
            pframe_t frame;

            if(!region)
              print("Unable to find fault region (?)\n");

            if(getMapping(aspace, faultAddr, &page) != 0)
            {
              frame = (pframe_t)(allocPhysPage() >> 12);
              setMapping(aspace, faultAddr, page);
            }
            else
              frame = page->physFrame;

 // XXX: Check error code to determine what to do
/* Allowed page faults:

   Access non-present memory, that exists in mapping
   Write to read-only memory marked as COW

   Not allowed:

   Access supervisor memory

   If region is marked as ZERO, then clear page
   If region is marked as COW and a write was performed, create new page, map it as r/w, and copy data
*/
            sys_map((addr_t)aspace->physAddr, faultAddr, frame, 1,
                    (region->flags & REG_RO) ? PM_READ_ONLY : PM_READ_WRITE);

            if(region->flags & REG_GUARD)
            {
              region->flags &= ~REG_GUARD;

              // XXX: This should only be done if the next guard page is unmapped
              // XXX: otherwise terminate the thread

              mapRegion(aspace, (faultAddr & ~(PAGE_SIZE-1))
                         + ((region->flags & REG_DOWN) ? -PAGE_SIZE : PAGE_SIZE),
                        NULL_PADDR, 1, REG_GUARD | (region->flags & REG_DOWN));
            }
            startThread(tid);
          }
          else
            print("Fault address not found in mapping.\n");
        }
//        print("\n");
        break;
      }
      case EXIT_MSG:
      {
        struct ExitMessage *exitMsg = (struct ExitMessage *)&msg->data;
        print("TID ");
        printInt(exitMsg->who);
        print(" exited with status code: ");
        printInt(exitMsg->statusCode);
        break;
       }
      default:
        print("Unhandled message with subject: "), printInt(msg->subject), print("\n");
        break;
    }
  }
  else
  {
    msg_t responseMsg;

    switch(msg->subject)
    {
      case MAP_MEM:
        break;
      case UNMAP_MEM:
        break;
      case CREATE_PORT:
        break;
      case DESTROY_PORT:
        break;
      case SEND_MESSAGE:
        break;
      case RECEIVE_MESSAGE:
        break;
      case REGISTER_SERVER:
      {
        struct ServerEntry *entry = malloc(sizeof(struct ServerEntry));
        struct RegisterServerRequest *request = (struct RegisterServerRequest *)&msg->data;
        char *key = malloc(6);

        entry->tid = msg->sender;
        entry->type = request->type;

        itoa(msg->sender, key, 10);

        if(!entry || sbHashInsert(&serverTable, key, entry) != 0)
          responseMsg.subject = RESPONSE_FAIL;
        else
          responseMsg.subject = RESPONSE_OK;
        break;
      }
      case UNREGISTER_SERVER:
      {
        struct ServerEntry *entry;
        char key[6];
        char *storedKey;

        itoa(msg->sender, key, 10);

        if(!entry || sbHashRemovePair(&serverTable, key, &storedKey, (void **)&entry) != 0)
          responseMsg.subject = RESPONSE_FAIL;
        else
        {
          free(storedKey);
          free(entry);
          responseMsg.subject = RESPONSE_OK;
        }
        break;
      }
      case REGISTER_NAME:
        nameRegister(msg, &responseMsg);
        break;
      case LOOKUP_NAME:
        nameLookup(msg, &responseMsg);
        break;
      case UNREGISTER_NAME:
        nameUnregister(msg, &responseMsg);
        break;
      default:
        responseMsg.subject = RESPONSE_FAIL;
        break;
    }

    responseMsg.recipient = msg->sender;

    sys_send(&responseMsg, 0);
  }
}

tid_t createNewThread(tid_t desiredTid, addr_t entry, addr_t pageMap, addr_t stackTop)
{
  struct AddrSpace *addrSpace = lookupPageMap(pageMap);
  tid_t tid = sys_create_thread(desiredTid, entry, pageMap, stackTop);

  if(tid == NULL_TID || attachTid(addrSpace, tid) != 0)
    return NULL_TID;
  else
    return tid;
}

static int loadElfExe(module_t *module)
{
  unsigned phtab_count;
  elf_header_t *image=NULL;
  elf_pheader_t *pheader;
  tid_t tid;
  int fail=0;
  addr_t stackTop = (addr_t)STACK_TOP;
  size_t modSize = module->mod_end-module->mod_start;

  if(!module)
    return -1;

  image = malloc(modSize);

  if(!image)
    return -1;

  if(peek(module->mod_start, (void *)image, modSize) != ESYS_OK)
  {
    if(image)
      free(image);

    return -1;
  }

  if(!isValidElfExe(image))
  {
    print("Not a valid ELF executable.\n");

    if(image)
      free(image);

    return -1;
  }

  u32 pmap = allocPhysPage();
  struct AddrSpace *addrSpace = malloc(sizeof(struct AddrSpace));

  if(!pmap || !addrSpace)
    fail = 1;
  else
  {
    initAddrSpace(addrSpace, (paddr_t)pmap);
    addAddrSpace(addrSpace);

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
    tid = createNewThread(NULL_TID, (addr_t)image->entry, pmap, stackTop);

    if(tid == NULL_TID)
      fail = 1;
    else if(mapRegion(addrSpace, STACK_TOP-PAGE_SIZE, NULL_PADDR, 1, REG_DOWN | REG_GUARD) != 0)
      fail = 1;
    else if(startThread(tid) != 0)
      fail = 1;
  }

  if(fail)
  {
    print("Failure\n");

    if(tid)
      detachTid(tid);

    if(addrSpace)
    {
      destroyAddrSpace(addrSpace);
      free(addrSpace);
    }

    if(pmap)
      freePhysPage(pmap);

    if(image)
      free(image);

    return -1;
  }

  if(image)
    free(image);

  return 0;
}

int startThread(tid_t tid)
{
  thread_info_t threadInfo =
  {
    .status = TCB_STATUS_READY
  };

  return sys_update_thread(tid, TF_STATUS, &threadInfo) == ESYS_OK ? 0 : -1;
}

int setPriority(tid_t tid, int priority)
{
  thread_info_t threadInfo =
  {
    .priority = priority
  };

  return sys_update_thread(tid, TF_PRIORITY, &threadInfo) == ESYS_OK ? 0 : -1;
}

int main(multiboot_info_t *info, addr_t lastFreeKernelPage)
{
  print("init server started.\n");

  if(initPageStack(info, lastFreeKernelPage) != 0)
    print("Unable to initialize the free page stack.\n");

  thread_info_t inInfo;

  sys_read_thread(NULL_TID, TF_PMAP, &inInfo);

  pageTable = malloc(0x100000*sizeof(page_t));

  if(!pageTable)
    print("Unable to allocate memory for page table\n");

  for(int i=0; i < 0x100000; i++)
  {
    pageTable[i].physFrame = i;
    pageTable[i].device = pageTable[i].block = pageTable[i].offset = pageTable[i].lastAccessed = 0;
    pageTable[i].flags = pageTable[i].isOnDisk = pageTable[i].isDiskPage = pageTable[i].isDirty = 0;
  }

  sbHashCreate(&addrSpaces, 4096);
  sbHashCreate(&threadNames, 4096);
  sbHashCreate(&deviceNames, 512);
  sbHashCreate(&fsNames, 128);
  sbHashCreate(&deviceTable, 512);
  sbHashCreate(&fsTable, 128);
  sbHashCreate(&serverTable, 1024);

  initAddrSpace(&initsrvAddrSpace, inInfo.rootPageMap);
  addAddrSpace(&initsrvAddrSpace);

  tid_t idleTid = createNewThread(NULL_TID, (addr_t)&idle, NULL, 0x00);

  if(idleTid == NULL_TID || setPriority(idleTid, 0) != 0 || startThread(idleTid) != 0)
  {
    print("Unable to start idle thread\n");
    return 1;
  }

  multiboot_info_t multibootStruct;
  module_t module;

  peek((addr_t)info, &multibootStruct, sizeof multibootStruct);

  // XXX: assume the first module is the initial server (and skip it)

  for(unsigned i=1; i < multibootStruct.mods_count; i++)
  {
    peek((addr_t)multibootStruct.mods_addr + i*sizeof(module_t), &module, sizeof(module_t));

    if(loadElfExe(&module) != 0)
      print("Unable to load elf module "), printInt(i), print("\n");
  }

  while(1)
  {
    msg_t msg =
    {
      .sender = ANY_SENDER
    };
    int code;

    if((code=sys_receive(&msg, 1)) == ESYS_OK)
      handleMessage(&msg);
    else
    {
      print("sys_receive() failed with code: "), printInt(code), print("\n");
      sys_wait(0);
    }
  }

  return -1;
}
