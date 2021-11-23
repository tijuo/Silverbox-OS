#include <kernel/thread.h>
#include <kernel/multiboot.h>
#include <kernel/mm.h>
#include <kernel/thread.h>
#include <elf.h>
#include "init.h"
#include "memory.h"

#define INIT_SERVER_FLAG  "--init"

DISC_CODE int loadServers(multiboot_info_t *info);
DISC_CODE static tcb_t* loadElfExe(addr_t, uint32_t, void*);
DISC_CODE static bool isValidElfExe(elf_header_t *image);
DISC_CODE static void bootstrapInitServer(multiboot_info_t *info);
DISC_DATA static addr_t initServerImg;

extern tcb_t *initServerThread;

int loadServers(multiboot_info_t *info) {
  module_t *module;
  bool initServerFound = false;

  kprintf("Boot info struct: %#p\nModules located at %#p. %d modules found.\n",
          (void*)KVIRT_TO_PHYS(info), info->mods_addr, info->mods_count);

  module = (module_t*)KPHYS_TO_VIRT(info->mods_addr);

  /* Locate the initial server module. */

  for(size_t i = info->mods_count; i; i--, module++) {
    if(i == info->mods_count)
      initServerImg = (addr_t)module->mod_start;

    if(module->string) {
      const char *strPtr = (const char*)KPHYS_TO_VIRT(module->string);

      do {
        strPtr = (const char*)strstr(strPtr, INIT_SERVER_FLAG);
        const char *separator = strPtr + strlen(INIT_SERVER_FLAG);

        if(*separator == '\0' || (*separator >= '\t' && *separator <= '\r')) {
          initServerFound = true;
          initServerImg = (addr_t)module->mod_start;
          break;
        }
        else
          strPtr = separator;
      } while(strPtr);
    }
  }

  if(!initServerFound) {
    if(info->mods_count)
      kprintf(
          "Initial server was not specified with --init. Using first module.\n");
    else {
      kprintf("Can't find initial server.");
      return E_FAIL;
    }
  }

  bootstrapInitServer(info);

  return E_OK;
}

bool isValidElfExe(elf_header_t *image) {
  return image && VALID_ELF(image)
         && image->identifier[EI_VERSION] == EV_CURRENT
         && image->type == ET_EXEC && image->machine == EM_386
         && image->version == EV_CURRENT
         && image->identifier[EI_CLASS] == ELFCLASS32;
}

tcb_t* loadElfExe(addr_t img, addr_t addrSpace, void *uStack) {
  elf_header_t image;
  elf_sheader_t sheader;
  tcb_t *thread;
  addr_t page;
  pde_t pde;
  pte_t pte;
  size_t i;
  size_t offset;

  peek(img, &image, sizeof image);
  pte.isPresent = 0;

  if(!isValidElfExe(&image)) {
    kprintf("Not a valid ELF executable.\n");
    return NULL;
  }

  thread = createThread((void*)image.entry, addrSpace, uStack);

  if(thread == NULL) {
    kprintf("loadElfExe(): Couldn't create thread.\n");
    return NULL;
  }

  /* Create the page table before mapping memory */

  for(i = 0; i < image.shnum; i++) {
    peek((img + image.shoff + i * image.shentsize), &sheader, sizeof sheader);

    if(!(sheader.flags & SHF_ALLOC))
      continue;

    for(offset = 0; offset < sheader.size; offset += PAGE_SIZE) {
      pde = readPDE(PDE_INDEX(sheader.addr + offset), addrSpace);

      if(!pde.isPresent) {
        page = allocPageFrame();

        if(page == INVALID_PFRAME)
          stopInit("loadElfExec(): Unable to allocate PDE.");

        clearPhysPage(page);

        pde.base = (uint32_t)ADDR_TO_PFRAME(page);
        pde.isReadWrite = 1;
        pde.isUser = 1;
        pde.isPresent = 1;

        if(IS_ERROR(writePDE(PDE_INDEX(sheader.addr + offset), pde, addrSpace)))
          RET_MSG(NULL, "loadElfExe(): Unable to write PDE.");
      }
      else if(pde.isLargePage)
        RET_MSG(
            NULL,
            "loadElfExe(): Memory region has already been mapped to a large page.");

      pte = readPTE(PTE_INDEX(sheader.addr + offset), PDE_BASE(pde));

      if(!pte.isPresent) {
        pte.isUser = 1;
        pte.isReadWrite = IS_FLAG_SET(sheader.flags, SHF_WRITE);
        pte.isPresent = 1;

        if(sheader.type == SHT_PROGBITS)
          pte.base = (uint32_t)ADDR_TO_PFRAME(
              (uint32_t )img + sheader.offset + offset);
        else if(sheader.type == SHT_NOBITS) {
          page = allocPageFrame();

          if(page == INVALID_PFRAME)
            stopInit("loadElfExe(): No more physical pages are available.");

          clearPhysPage(page);
          pte.base = (uint32_t)ADDR_TO_PFRAME(page);
        }
        else
          continue;

        if(IS_ERROR(
            writePTE(PTE_INDEX(sheader.addr + offset), pte, PDE_BASE(pde))))
          RET_MSG(NULL, "loadElfExe(): Unable to write PDE");
      }
      else if(sheader.type == SHT_NOBITS)
        memset((void*)(sheader.addr + offset), 0,
        PAGE_SIZE - (offset % PAGE_SIZE));
    }
  }

  return thread;
}

struct InitStackArgs {
  uint32_t returnAddress;
  multiboot_info_t *multibootInfo;
  addr_t firstFreePage;
  size_t stackSize;
  unsigned char code[4];
};

/**
 Bootstraps the initial server and passes necessary boot data to it.
 */

void bootstrapInitServer(multiboot_info_t *info) {
  addr_t initServerStack = (addr_t)INIT_SERVER_STACK_TOP;
  addr_t initServerPDir = INVALID_PFRAME;
  elf_header_t elf_header;

  /* code:

   xor    eax, eax
   xor    ebx, ebx
   inc    ebx
   int    0xFF     # sys_exit(1)
   nop
   nop
   nop
   ud2             # Trigger Invalid Opcode Exception: #UD
   */

  struct InitStackArgs stackData = {
    .returnAddress = initServerStack - sizeof((struct InitStackArgs*)0)->code,
    .multibootInfo = info,
    .firstFreePage = firstFreePage,
    .stackSize = 0,
    .code = {
      0x90,
      0x90,
      0x0F,
      0x0B
    }
  };

  kprintf("Bootstrapping initial server...\n");

  peek(initServerImg, &elf_header, sizeof elf_header);

  if(!isValidElfExe(&elf_header)) {
    kprintf("Invalid ELF exe\n");
    goto failedBootstrap;
  }

  if((initServerPDir = allocPageFrame()) == INVALID_PFRAME) {
    kprintf("Unable to create page directory for initial server.\n");
    goto failedBootstrap;
  }

  if(clearPhysPage(initServerPDir) != E_OK) {
    kprintf("Unable to clear init server page directory.\n");
    goto failedBootstrap;
  }

  pde_t pde;
  pte_t pte;

  memset(&pde, 0, sizeof(pde_t));
  memset(&pte, 0, sizeof(pte_t));

  addr_t stackPTab = allocPageFrame();

  if(stackPTab == INVALID_PFRAME) {
    kprintf("Unable to initialize stack page table\n");
    goto failedBootstrap;
  }
  else
    clearPhysPage(stackPTab);

  pde.base = (uint32_t)ADDR_TO_PFRAME(stackPTab);
  pde.isReadWrite = 1;
  pde.isUser = 1;
  pde.isPresent = 1;

  if(IS_ERROR(
      writePDE(PDE_INDEX(initServerStack-PAGE_TABLE_SIZE), pde, initServerPDir)))
  {
    goto failedBootstrap;
  }

  for(size_t p = 0; p < (512 * 1024) / PAGE_SIZE; p++) {
    addr_t stackPage = allocPageFrame();

    if(stackPage == INVALID_PFRAME) {
      kprintf("Unable to initialize stack page.\n");
      goto failedBootstrap;
    }

    pte.base = (uint32_t)ADDR_TO_PFRAME(stackPage);
    pte.isReadWrite = 1;
    pte.isUser = 1;
    pte.isPresent = 1;

    if(IS_ERROR(
        writePTE(PTE_INDEX(initServerStack-(p+1)*PAGE_SIZE), pte, stackPTab))) {
      if(p == 0) {
        kprintf("Unable to write page map entries for init server stack.\n");
        goto failedBootstrap;
      }
      else
        break;
    }

    if(p == 0) {
      poke(stackPage + PAGE_SIZE - sizeof(stackData), &stackData,
           sizeof(stackData));
    }

    stackData.stackSize += PAGE_SIZE;
  }

  if((initServerThread = loadElfExe(
      initServerImg, initServerPDir,
      (void*)(initServerStack - sizeof(stackData))))
     == NULL) {
    kprintf("Unable to load ELF executable.\n");
    goto failedBootstrap;
  }

  kprintf("Starting initial server... %#p\n", initServerThread);

  if(IS_ERROR(startThread(initServerThread)))
    goto releaseInitThread;

  return;

releaseInitThread:
  releaseThread(initServerThread);

failedBootstrap:
  kprintf("Unable to start initial server.\n");
  return;
}

