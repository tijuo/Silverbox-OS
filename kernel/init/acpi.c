#include "init.h"
#include "memory.h"
#include <stdint.h>
#include <kernel/apic.h>
#include <kernel/lowlevel.h>
#include <kernel/memory.h>
#include <types.h>

#define RSDP_SIGNATURE  "RSD PTR "
#define PARAGRAPH_LEN   16

struct rsdp_entry {
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

_Static_assert(sizeof(struct rsdp_entry) == 36, "rsdp_entry struct should be 36 bytes.");

struct acpi_dt_header {
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

_Static_assert(sizeof(struct acpi_dt_header) == 36, "acpi_dt_header should be 36 bytes.");

struct madt_header {
  struct acpi_dt_header dt_header;
  uint32_t lapic_address;
  uint32_t flags;
};

_Static_assert(sizeof(struct madt_header) == 44, "madt_header should be 44 bytes.");

struct ic_header {
  uint8_t type;
  uint8_t length;
};

#define TYPE_PROC_LAPIC           0
#define TYPE_IOAPIC               1
#define TYPE_IOAPIC_INT_OVERRIDE  2
#define TYPE_IOAPIC_NMI_SRC       3
#define TYPE_LAPIC_NMI_SRC        4
#define TYPE_LAPIC_ADDR_OVERRIDE  5
#define TYPE_X2APIC               9

struct lapic_header {
  struct ic_header header;
  uint8_t uid;
  uint8_t lapic_id;
  uint32_t flags;
};

_Static_assert(sizeof(struct lapic_header) == 8, "lapic_header should be 8 bytes.");

#define PROC_LAPIC_ENABLED    1
#define PROC_LAPIC_ONLINE     2

struct ioapic_header {
  struct ic_header header;
  uint8_t ioapic_id;
  uint8_t _resd;
  uint32_t ioapic_address;
  uint32_t global_sys_int_base;
};

_Static_assert(sizeof(struct ioapic_header) == 12, "lapic_header should be 12 bytes.");

#define APIC_ACTIVE_LOW     0x02
#define APIC_LEVEL_TRIG     0x08

struct ioapic_int_src_override {
  struct ic_header header;
  uint8_t bus_src;
  uint8_t irq_src;
  uint32_t global_sys_int;
  uint16_t flags;
} PACKED;

_Static_assert(sizeof(struct ioapic_int_src_override) == 10, "ioapic_int_src_override should be 10 bytes.");

struct ioapic_nmi_src {
  struct ic_header header;
  uint8_t nmi_src;
  uint8_t _resd;
  uint16_t flags;
  uint32_t global_sys_int;
} PACKED;

_Static_assert(sizeof(struct ioapic_nmi_src) == 10, "ioapic_nmi_src should be 10 bytes.");

struct lapic_nmi_src {
  struct ic_header header;
  uint8_t proc_id;
  uint16_t flags;
  uint8_t lint;
} PACKED;

_Static_assert(sizeof(struct lapic_nmi_src) == 6, "lapic_nmi_src should be 6 bytes.");

struct lapic_addr_override {
  struct ic_header header;
  uint16_t _resd;
  uint64_t lapic_addr;
} PACKED;

_Static_assert(sizeof(struct lapic_addr_override) == 12, "lapic_addr_override should be 12 bytes.");

#define X2APIC_ENABLED PROC_LAPIC_ENABLED
#define X2APIC_ONLINE PROC_LAPIC_ONLINE

struct x2apic {
  struct ic_header header;
  uint16_t _resd;
  uint32_t x2apic_id;
  uint32_t flags;
  uint32_t acpi_id;
};

_Static_assert(sizeof(struct x2apic) == 16, "x2apic should be 16 bytes.");

DISC_CODE static bool is_valid_acpi_header(paddr_t phys_address);
DISC_CODE int read_acpi_tables(void);

bool is_valid_acpi_header(paddr_t phys_address) {
  struct acpi_dt_header *header = (struct acpi_dt_header *)phys_address;
  uint8_t checksum = 0;
  size_t header_len = header->length;

  for(uint8_t *ptr = (uint8_t *)phys_address; header_len; header_len--, ptr++)
    checksum += *ptr;

  return checksum == 0 ? true : false;
}

int read_acpi_tables(void) {
  struct rsdp_entry *rsdp = NULL;
  size_t processors_found = 0;

  // Look for the RSDP

  for(uint8_t *ptr = (uint8_t *)BIOS_EXT_ROM; ptr < (uint8_t *)EXTENDED_MEMORY;
      ptr += PARAGRAPH_LEN) {
    if(kmemcmp(ptr, RSDP_SIGNATURE, sizeof rsdp->signature) == 0) {
      uint8_t checksum = 0;

      for(size_t i = 0; i < offsetof(struct rsdp_entry, length); i++)
        checksum += ptr[i];

      if(checksum == 0) {
        rsdp = (struct rsdp_entry *)ptr;
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
      kprintf(" XSDT at %p", (void *)rsdp->xsdt_address);
      use_xsdt = 1;
    }

    kprintf("\n");

    if((use_xsdt && is_valid_acpi_header(rsdp->xsdt_address))
        || (!use_xsdt && is_valid_acpi_header((paddr_t)rsdp->rsdt_address))) {

      struct acpi_dt_header *root_desc = (struct acpi_dt_header *)(
                                           use_xsdt ? rsdp->xsdt_address : (paddr_t)rsdp->rsdt_address);

      size_t entry_size = use_xsdt ? sizeof(uint64_t) : sizeof(uint32_t);
      size_t total_entries = (root_desc->length - sizeof(struct acpi_dt_header))
                             / entry_size;

      kprintf("%.4s found at %p\n", root_desc->signature, root_desc);

      for(uintptr_t entry_ptr = (uintptr_t)(root_desc + 1); total_entries;
          total_entries--, entry_ptr += entry_size) {
        if((use_xsdt && is_valid_acpi_header((paddr_t)*(uint64_t *)entry_ptr))
            || (!use_xsdt
                && is_valid_acpi_header((paddr_t)*(uint32_t *)entry_ptr))) {
          struct acpi_dt_header *header;

          if(use_xsdt)
            header = (struct acpi_dt_header *)*(uint64_t *)entry_ptr;
          else
            header = (struct acpi_dt_header *)(uintptr_t)*(uint32_t *)entry_ptr;

          kprintf("%.4s found at %p\n", header->signature, header);

          // If the MADT is in the RSDT, then retrieve apic data

          if(kmemcmp(&header->signature, "APIC", 4) == 0) {
            struct madt_header *madt_header = (struct madt_header *)header;
            size_t madt_offset = sizeof *madt_header;

            kprintf("Local APIC address: %#x\n", madt_header->lapic_address);
            lapic_ptr = (void *)(uintptr_t)madt_header->lapic_address;

            while(madt_offset < madt_header->dt_header.length) {
              struct ic_header *ic_header =
                (struct ic_header *)((uintptr_t)madt_header + madt_offset);

              switch(ic_header->type) {
                case TYPE_PROC_LAPIC: {

                    /*
                       Each processor has a local APIC. Use this to find each processor's
                       local APIC id.
                    */
                    struct lapic_header *proc_lapic_header =
                      (struct lapic_header *)ic_header;

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
                  }
                  break;
                case TYPE_IOAPIC: {
                    struct ioapic_header *ioapic_header =
                      (struct ioapic_header *)ic_header;

                    kprintf("I/O APIC id %u is at %#x. Global System Interrupt Base: %#x\n",
                            ioapic_header->ioapic_id, ioapic_header->ioapic_address,
                            ioapic_header->global_sys_int_base);
                    ioapic_ptr = (void *)(uintptr_t)ioapic_header->ioapic_address;
                  }
                  break;
                case TYPE_IOAPIC_INT_OVERRIDE: {
                    struct ioapic_int_src_override *int_src_override =
                      (struct ioapic_int_src_override *)ic_header;

                    kprintf("Bus: %u IRQ %u -> global system int: %#x", int_src_override->bus_src,
                            int_src_override->irq_src, int_src_override->global_sys_int);

                    if(IS_FLAG_SET(int_src_override->flags, APIC_ACTIVE_LOW))
                      kprintf(" active low");
                    else
                      kprintf(" active high");

                    if(IS_FLAG_SET(int_src_override->flags, APIC_LEVEL_TRIG))
                      kprintf(" level-triggered");
                    else
                      kprintf(" edge-triggered");

                    kprintf("\n");
                  }
                  break;
                case TYPE_IOAPIC_NMI_SRC: {
                    struct ioapic_nmi_src *ioapic_nmi_src = (struct ioapic_nmi_src *)ic_header;

                    kprintf("I/O APIC NMI IRQ: %u -> global system int: %#x", ioapic_nmi_src->nmi_src,
                            ioapic_nmi_src->global_sys_int);

                    if(IS_FLAG_SET(ioapic_nmi_src->flags, APIC_ACTIVE_LOW))
                      kprintf(" active low");
                    else
                      kprintf(" active high");

                    if(IS_FLAG_SET(ioapic_nmi_src->flags, APIC_LEVEL_TRIG))
                      kprintf(" level-triggered");
                    else
                      kprintf(" edge-triggered");

                    kprintf("\n");
                  }
                  break;
                case TYPE_LAPIC_NMI_SRC: {
                    struct lapic_nmi_src *lapic_nmi_src = (struct lapic_nmi_src *)ic_header;

                    kprintf("LINT%u processor id: %#x", lapic_nmi_src->lint, lapic_nmi_src->proc_id);

                    if(IS_FLAG_SET(lapic_nmi_src->flags, APIC_ACTIVE_LOW))
                      kprintf(" active low");
                    else
                      kprintf(" active high");

                    if(IS_FLAG_SET(lapic_nmi_src->flags, APIC_LEVEL_TRIG))
                      kprintf(" level-triggered");
                    else
                      kprintf(" edge-triggered");

                    kprintf("\n");
                  }
                  break;
                case TYPE_LAPIC_ADDR_OVERRIDE: {
                    struct lapic_addr_override *lapic_addr_override = (struct lapic_addr_override *)ic_header;

                    kprintf("Local APIC address: %p\n", (void *)lapic_addr_override->lapic_addr);
                  }
                  break;
                case TYPE_X2APIC: {
                    struct x2apic *x2apic = (struct x2apic *)ic_header;

                    kprintf("ACPI id: %#x X2APIC id: %#x%s\n", x2apic->acpi_id, x2apic->x2apic_id,
                            IS_FLAG_SET(x2apic->flags,
                                        X2APIC_ENABLED) ?
                            "" : IS_FLAG_SET(x2apic->flags,
                                             X2APIC_ONLINE) ? " (offline)" :
                            " (disabled)");
                  }
                  break;
                default:
                  kprintf("APIC Entry type %u found.\n", ic_header->type);
                  break;
              }

              madt_offset += ic_header->length;
            }

            kprintf("%lu processor%c found.\n", processors_found, processors_found != 1 ? 's' : '\0');
          }
        } else
          kprintf("Unable to read %4s entries at %p.\n", root_desc->signature,
                  root_desc);
      }
    } else
      kprintf("Unable to read %s\n", use_xsdt ? "XSDT" : "RSDT");
  } else
    kprintf("RSDP not found\n");

  return processors_found;
}
