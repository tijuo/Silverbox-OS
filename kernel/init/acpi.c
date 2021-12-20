#include "init.h"
#include "memory.h"
#include <stdint.h>
#include <kernel/apic.h>
#include <kernel/lowlevel.h>
#include <kernel/memory.h>
#include <oslib.h>
#include <types.h>

#define RSDP_SIGNATURE  "RSD PTR "
#define PARAGRAPH_LEN   16

struct RSDPointer {
  char signature[8]; // should be "RSD PTR "
  uint8_t checksum;
  char oem_id[6];
  uint8_t revision;
  uint32_t rsdt_address; // physical address
  uint32_t length;
  uint64_t xsdt_address; // physical address
  uint8_t ext_checksum;
  char _resd[3];
} PACKED;

_Static_assert(sizeof(struct RSDPointer) == 36, "RSDPointer struct should be 36 bytes.");

struct ACPI_DT_Header {
  char signature[4];
  uint32_t length;
  uint8_t revision;
  uint8_t checksum;
  char oem_id[6];
  char oem_table_id[8];
  uint32_t oem_revision;
  uint32_t creator_id;
  uint32_t creator_revision;
};

_Static_assert(sizeof(struct ACPI_DT_Header) == 36, "ACPI_DT_Header should be 36 bytes.");

struct MADT_Header {
  struct ACPI_DT_Header dt_header;
  uint32_t lapic_address;
  uint32_t flags;
};

_Static_assert(sizeof(struct MADT_Header) == 44, "MADT_Header should be 44 bytes.");

struct IC_Header {
  uint8_t type;
  uint8_t length;
};

#define TYPE_PROC_LAPIC     0
#define TYPE_IOAPIC         1

struct ProcessorLAPIC_Header {
  struct IC_Header header;
  uint8_t uid;
  uint8_t lapic_id;
  uint32_t flags;
};

_Static_assert(sizeof(struct ProcessorLAPIC_Header) == 8, "ProcessorLAPIC_Header should be 8 bytes.");

#define PROC_LAPIC_ENABLED    1
#define PROC_LAPIC_ONLINE     2

struct IOAPIC_Header {
  struct IC_Header header;
  uint8_t ioapic_id;
  uint8_t _resd;
  uint32_t ioapic_address;
  uint32_t global_sys_int_base;
};

_Static_assert(sizeof(struct IOAPIC_Header) == 12, "IOAPIC_Header should be 12 bytes.");

DISC_CODE static bool is_valid_acpi_header(paddr_t phys_address);
DISC_CODE int read_acpi_tables(void);

bool is_valid_acpi_header(paddr_t phys_address) {
  struct ACPI_DT_Header *header = (struct ACPI_DT_Header*)phys_address;
  uint8_t checksum = 0;
  size_t header_len = header->length;

  for(uint8_t *ptr = (uint8_t*)phys_address; header_len; header_len--, ptr++)
    checksum += *ptr;

  return checksum == 0 ? true : false;
}

int read_acpi_tables(void) {
  struct RSDPointer *rsdp = NULL;
  size_t processors_found = 0;

  // Look for the RSDP

  for(uint8_t *ptr = (uint8_t*)BIOS_EXT_ROM; ptr < (uint8_t*)EXTENDED_MEMORY;
      ptr += PARAGRAPH_LEN)
  {
    if(memcmp(ptr, RSDP_SIGNATURE, sizeof rsdp->signature) == 0) {
      uint8_t checksum = 0;

      for(size_t i = 0; i < offsetof(struct RSDPointer, length); i++)
        checksum += ptr[i];

      if(checksum == 0) {
        rsdp = (struct RSDPointer*)ptr;
        break;
      }
    }
  }

  if(rsdp) {
    int use_xsdt = 0;

    // Now go through the RSDT, looking for the MADT

    kprintf("ACPI rev. %u %.8s found at %p. RSDT at %#x", rsdp->revision,
        rsdp->signature, rsdp, rsdp->rsdt_address);

    if(rsdp->revision >= 2) {
      kprintf(" XSDT at %p", (void*)rsdp->xsdt_address);
      use_xsdt = 1;
    }

    kprintf("\n");

    if((use_xsdt && is_valid_acpi_header(rsdp->xsdt_address))
        || (!use_xsdt && is_valid_acpi_header((paddr_t)rsdp->rsdt_address))) {

      struct ACPI_DT_Header *root_desc = (struct ACPI_DT_Header*)(
          use_xsdt ? rsdp->xsdt_address : (paddr_t)rsdp->rsdt_address);

      size_t entry_size = use_xsdt ? sizeof(uint64_t) : sizeof(uint32_t);
      size_t total_entries = (root_desc->length - sizeof(struct ACPI_DT_Header))
        / entry_size;

      kprintf("%.4s found at %p\n", root_desc->signature, root_desc);

      for(uintptr_t entry_ptr = (uintptr_t)(root_desc + 1); total_entries;
          total_entries--, entry_ptr += entry_size)
      {
        if((use_xsdt && is_valid_acpi_header((paddr_t)*(uint64_t*)entry_ptr))
            || (!use_xsdt
              && is_valid_acpi_header((paddr_t)*(uint32_t*)entry_ptr))) {
          struct ACPI_DT_Header *header;

          if(use_xsdt)
            header = (struct ACPI_DT_Header*)*(uint64_t*)entry_ptr;
          else
            header = (struct ACPI_DT_Header*)(uintptr_t)*(uint32_t*)entry_ptr;

          kprintf("%.4s found at %p\n", header->signature, header);

          // If the MADT is in the RSDT, then retrieve apic data

          if(memcmp(&header->signature, "APIC", 4) == 0) {
            struct MADT_Header *madt_header = (struct MADT_Header*)header;
            size_t madt_offset = sizeof *madt_header;

            kprintf("Local APIC address: %#x\n", madt_header->lapic_address);
            lapic_ptr = (void*)(uintptr_t)madt_header->lapic_address;

            while(madt_offset < madt_header->dt_header.length) {
              struct IC_Header *ic_header =
                (struct IC_Header*)((uintptr_t)madt_header + madt_offset);

              switch(ic_header->type) {
                case TYPE_PROC_LAPIC: {

/*
   Each processor has a local APIC. Use this to find each processor's
   local APIC id.
*/
                  struct ProcessorLAPIC_Header *proc_lapic_header =
                    (struct ProcessorLAPIC_Header*)ic_header;

                  if(IS_FLAG_SET(proc_lapic_header->flags,
                                 PROC_LAPIC_ENABLED) ||
                     IS_FLAG_SET(proc_lapic_header->flags,
                                 PROC_LAPIC_ONLINE)) {
                    kprintf("Processor %u has local APIC id: %u%s\n",
                                 proc_lapic_header->uid,
                                 proc_lapic_header->lapic_id,
                                 IS_FLAG_SET(proc_lapic_header->flags,
                                             PROC_LAPIC_ENABLED) ?
                                              "" : IS_FLAG_SET(proc_lapic_header->flags,
                                                PROC_LAPIC_ONLINE) ? " (offline)" :
                                              " (disabled)");
                    processors[processors_found].local_apic_id = proc_lapic_header
                                            ->lapic_id;
                    processors[processors_found].acpi_uid = proc_lapic_header
                                            ->uid;
                    processors[processors_found].is_online = !!IS_FLAG_SET(
                                              proc_lapic_header->flags, PROC_LAPIC_ENABLED);

                    processors_found++;
                  }
                  break;
                }
                case TYPE_IOAPIC: {
                  struct IOAPIC_Header *ioapic_header =
                                      (struct IOAPIC_Header*)ic_header;

                  kprintf("IOAPIC id %u is at %#x. Global System Interrupt Base: %#x\n",
                    ioapic_header->ioapic_id, ioapic_header->ioapic_address,
                    ioapic_header->global_sys_int_base);
                    ioapic_ptr = (void*)(uintptr_t)ioapic_header->ioapic_address;
                  break;
                }
                default:
                  kprintf("APIC Entry type %u found.\n", ic_header->type);
                  break;
              }

              madt_offset += ic_header->length;
            }

            kprintf("%u processor%c found.\n", processors_found, processors_found != 1 ? 's' : '\0');
          }
        }
        else
          kprintf("Unable to read %4s entries at %p.\n", root_desc->signature,
              root_desc);
      }
    }
    else
      kprintf("Unable to read %s\n", use_xsdt ? "XSDT" : "RSDT");
  }
  else
    kprintf("RSDP not found\n");

  return processors_found;
}
