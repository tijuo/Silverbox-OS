#include "init.h"
#include "memory.h"
#include <stdint.h>
#include <types.h>
#include <kernel/apic.h>
#include <kernel/lowlevel.h>
#include <kernel/memory.h>
#include <oslib.h>

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
};

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

#define PROC_LAPIC_ENABLED    1
#define PROC_LAPIC_ONLINE     2

struct IOAPIC_Header {
  struct IC_Header header;
  uint8_t ioapic_id;
  uint8_t _resd;
  uint32_t ioapic_address;
  uint32_t global_sys_int_base;
};

DISC_CODE static bool is_valid_acpi_header(paddr_t phys_address);
DISC_CODE int read_acpi_tables(void);

bool is_valid_acpi_header(paddr_t phys_address) {
  struct ACPI_DT_Header *header = (struct ACPI_DT_Header *)phys_address;
  uint8_t checksum = 0;
  size_t header_len = header->length;

  for(uint8_t *ptr=(uint8_t *)phys_address; header_len; header_len--, ptr++)
	checksum += *ptr;

  return checksum == 0 ? true : false;
}

int read_acpi_tables(void) {
  struct RSDPointer *rsdp = NULL;
  size_t processors_found = 0;

  // Look for the RSDP

  for(uint8_t *ptr = (uint8_t*)BIOS_EXT_ROM;
      ptr < (uint8_t*)EXTENDED_MEMORY; ptr += PARAGRAPH_LEN)
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
    // Now go through the RSDT, looking for the MADT

    kprintf("%.8s found at %p. RSDT at %p\n", rsdp->signature, rsdp,
            (void *)rsdp->rsdt_address);

    if(rsdp->rsdt_address && is_valid_acpi_header((paddr_t)rsdp->rsdt_address)) {
      struct ACPI_DT_Header *rsdt = (struct ACPI_DT_Header *)(paddr_t)rsdp->rsdt_address;

      size_t total_entries = (rsdt->length - sizeof(struct ACPI_DT_Header))
          / sizeof(uint32_t);

      kprintf("%.4s found at %p\n", rsdt->signature, rsdt);

      for(uint32_t *rsdt_entry=(uint32_t *)(rsdt+1); total_entries; rsdt_entry++, total_entries--) {
        if(is_valid_acpi_header((paddr_t)*rsdt_entry)) {
          struct ACPI_DT_Header *header = (struct ACPI_DT_Header *)*rsdt_entry;

          kprintf("%.4s found at %p\n", header->signature, header);

          // If the MADT is in the RSDT, then retrieve apic data

          if(memcmp(&header->signature, "APIC", 4) == 0) {
            struct MADT_Header *madt_header=(struct MADT_Header *)header;
			size_t madt_offset = sizeof madt_header;

            kprintf("Local APIC address: %#x\n", madt_header->lapic_address);
            lapic_ptr = (void *)madt_header->lapic_address;

            while(madt_offset < madt_header->dt_header.length)
            {
              struct IC_Header *ic_header = (struct IC_Header *)((uintptr_t)madt_header + madt_offset);

              switch(ic_header->type) {
                case TYPE_PROC_LAPIC: {
                  /* Each processor has a local APIC. Use this to find each processor's
                   local APIC id. */
                  struct ProcessorLAPIC_Header *proc_lapic_header = (struct ProcessorLAPIC_Header *)ic_header;

                  if(IS_FLAG_SET(proc_lapic_header->flags, PROC_LAPIC_ENABLED) || IS_FLAG_SET(
                      proc_lapic_header->flags, PROC_LAPIC_ONLINE)) {
                    kprintf(
                        "Processor %u has local APIC id: %u%s\n",
                        proc_lapic_header->uid,
                        proc_lapic_header->lapic_id,
                        IS_FLAG_SET(proc_lapic_header->flags, PROC_LAPIC_ENABLED) ?
                            "" :
                        IS_FLAG_SET(proc_lapic_header->flags, PROC_LAPIC_ONLINE) ?
                            " (offline)" : " (disabled)");
                    processors[processors_found].lapic_id = proc_lapic_header->lapic_id;
                    processors[processors_found].acpi_uid = proc_lapic_header->uid;
                    processors[processors_found].is_online = !!IS_FLAG_SET(
                        proc_lapic_header->flags, PROC_LAPIC_ENABLED);

                    processors_found++;
                  }
                  break;
                }
                case TYPE_IOAPIC: {
                  struct IOAPIC_Header *ioapic_header = (struct IOAPIC_Header *)header;

                  kprintf(
                      "IOAPIC id %u is at %p. Global System Interrupt Base: %#x\n",
                      ioapic_header->ioapic_id, (void *)ioapic_header->ioapic_address,
                      ioapic_header->global_sys_int_base);
                  ioapic_ptr = (void *)ioapic_header->ioapic_address;
                  break;
                }
                default:
                  kprintf("APIC Entry type %u found.\n", ic_header->type);
                  break;
              }

              madt_offset += ic_header->length;
            }

            kprintf("%u processors found.\n", processors_found);
          }
        }
        else
          kprintf("Unable to read RSDT entries at %p.\n", (void *)*rsdt_entry);
      }
    }
    else
      kprintf("Unable to read RSDT\n");
  }
  else
    kprintf("RSDP not found\n");

  return processors_found;
}
