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

DISC_CODE static bool is_valid_acpi_header(uint64_t phys_address);
DISC_CODE int read_acpi_tables(void);

bool is_valid_acpi_header(uint64_t phys_address)
{
    struct ACPI_DT_Header header;
    uint8_t checksum = 0;
    uint8_t buffer[128];
    size_t bytes_read = 0;
    size_t header_len;

    if(phys_address >= MAX_PHYS_MEMORY)
        RET_MSG(false, "Physical memory is out of range.");

    if(IS_ERROR(peek(phys_address, &header, sizeof header)))
        RET_MSG(false, "Unable to read ACPI descriptor table header.");

    header_len = header.length;

    memcpy(buffer, &header, sizeof header);

    for(size_t i = 0; i < sizeof header; i++, bytes_read++)
        checksum += buffer[i];

    while(bytes_read < header_len) {
        size_t bytes_to_read = MIN(sizeof buffer, header_len - bytes_read);

        if(IS_ERROR(peek(phys_address + bytes_read, buffer, bytes_to_read)))
            RET_MSG(false, "Unable to read ACPI descriptor table header.");

        for(size_t i = 0; i < bytes_to_read; i++, bytes_read++)
            checksum += buffer[i];
    }

    return checksum == 0 ? true : false;
}

int read_acpi_tables(void)
{
    struct RSDPointer rsdp;
    addr_t rsdp_addr = 0;
    size_t processors_found = 0;

    // Look for the RSDP

    for(uint8_t* ptr = (uint8_t*)KPHYS_TO_VIRT(BIOS_EXT_ROM);
        ptr < (uint8_t*)KPHYS_TO_VIRT(EXTENDED_MEMORY); ptr += PARAGRAPH_LEN) {
        if(memcmp(ptr, RSDP_SIGNATURE, sizeof rsdp.signature) == 0) {
            uint8_t checksum = 0;

            for(size_t i = 0; i < offsetof(struct RSDPointer, length); i++)
                checksum += ptr[i];

            if(checksum == 0) {
                rsdp = *(struct RSDPointer*)ptr;
                rsdp_addr = (addr_t)ptr;
                break;
            }
        }
    }

    if(rsdp_addr) {
        // Now go through the RSDT, looking for the MADT

        kprintf("%.8s found at %#x. RSDT at %#x\n", rsdp.signature, rsdp_addr,
            rsdp.rsdt_address);

        if(rsdp.rsdt_address && is_valid_acpi_header((uint64_t)rsdp.rsdt_address)) {
            struct ACPI_DT_Header rsdt;

            if(IS_ERROR(peek((uint64_t)rsdp.rsdt_address, &rsdt, sizeof rsdt))) {
                kprintf("Unable to read RSDT\n");
                return E_FAIL;
            }

            size_t total_entries = (rsdt.length - sizeof(struct ACPI_DT_Header))
                / sizeof(uint32_t);

            kprintf("%.4s found at %#x\n", rsdt.signature, rsdp.rsdt_address);

            for(size_t rsdt_index = 0; rsdt_index < total_entries; rsdt_index++) {
                uint32_t rsdt_entry_ptr;

                if(IS_ERROR(
                    peek(
                        (uint64_t)rsdp.rsdt_address + sizeof rsdt
                        + rsdt_index * sizeof(uint32_t),
                        &rsdt_entry_ptr, sizeof(uint32_t)))) {
                    kprintf("Unable to read descriptor table.\n");
                    return E_FAIL;
                }

                if(is_valid_acpi_header((uint64_t)rsdt_entry_ptr)) {
                    struct ACPI_DT_Header header;

                    if(IS_ERROR(peek((uint64_t)rsdt_entry_ptr, &header, sizeof header))) {
                        kprintf("Unable to read descriptor table header.\n");
                        continue;
                    }

                    kprintf("%.4s found at %#x\n", header.signature, rsdt_entry_ptr);

                    // If the MADT is in the RSDT, then retrieve apic data

                    if(memcmp(&header.signature, "APIC", 4) == 0) {
                        struct MADT_Header madt_header;

                        if(IS_ERROR(
                            peek((uint64_t)rsdt_entry_ptr, &madt_header, sizeof madt_header))) {
                            kprintf("Unable to read MADT header.\n");
                            return E_FAIL;
                        }

                        kprintf("Local APIC address: %#x\n", madt_header.lapic_address);
                        lapic_ptr = madt_header.lapic_address;

                        for(size_t madt_offset = sizeof madt_header;
                            madt_offset < madt_header.dt_header.length;) {
                            struct IC_Header ic_header;

                            if(IS_ERROR(
                                peek((uint64_t)rsdt_entry_ptr + madt_offset, &ic_header,
                                    sizeof ic_header))) {
                                kprintf("Unable to read interrupt controller header.\n");
                                return E_FAIL;
                            }

                            switch(ic_header.type) {
                                case TYPE_PROC_LAPIC:
                                {
                                    /* Each processor has a local APIC. Use this to find each processor's
                                     local APIC id. */
                                    struct ProcessorLAPIC_Header proc_lapic_header;

                                    if(IS_ERROR(
                                        peek((uint64_t)rsdt_entry_ptr + madt_offset,
                                            &proc_lapic_header, sizeof proc_lapic_header))) {
                                        kprintf("Unable to read processor local APIC header.\n");
                                        return E_FAIL;
                                    }

                                    if(IS_FLAG_SET(proc_lapic_header.flags, PROC_LAPIC_ENABLED) || IS_FLAG_SET(
                                        proc_lapic_header.flags, PROC_LAPIC_ONLINE)) {
                                        kprintf(
                                            "Processor %d has local APIC id: %d%s\n",
                                            proc_lapic_header.uid,
                                            proc_lapic_header.lapic_id,
                                            IS_FLAG_SET(proc_lapic_header.flags, PROC_LAPIC_ENABLED) ?
                                            "" :
                                            IS_FLAG_SET(proc_lapic_header.flags, PROC_LAPIC_ONLINE) ?
                                            " (offline)" : " (disabled)");
                                        processors[processors_found].lapic_id = proc_lapic_header.lapic_id;
                                        processors[processors_found].acpi_uid = proc_lapic_header.uid;
                                        processors[processors_found].is_online = !!IS_FLAG_SET(
                                            proc_lapic_header.flags, PROC_LAPIC_ENABLED);

                                        processors_found++;
                                    }
                                    break;
                                }
                                case TYPE_IOAPIC:
                                {
                                    struct IOAPIC_Header ioapic_header;

                                    if(IS_ERROR(
                                        peek((uint64_t)rsdt_entry_ptr + madt_offset, &ioapic_header,
                                            sizeof ioapic_header))) {
                                        kprintf("Unable to read processor local APIC header.\n");
                                        return E_FAIL;
                                    }

                                    kprintf(
                                        "IOAPIC id %d is at %#x. Global System Interrupt Base: %#x\n",
                                        ioapic_header.ioapic_id, ioapic_header.ioapic_address,
                                        ioapic_header.global_sys_int_base);
                                    ioapic_ptr = ioapic_header.ioapic_id;
                                    break;
                                }
                                default:
                                    kprintf("APIC Entry type %d found.\n", ic_header.type);
                                    break;
                            }

                            madt_offset += ic_header.length;
                        }

                        kprintf("%d processors found.\n", processors_found);
                    }
                } else
                    kprintf("Unable to read RSDT entries at %#x\n", rsdt_entry_ptr);
            }
        } else
            kprintf("Unable to read RSDT\n");
    } else
        kprintf("RSDP not found\n");

    return processors_found;
}
