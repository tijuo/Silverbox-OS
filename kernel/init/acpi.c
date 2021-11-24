#include "init.h"
#include "memory.h"
#include <stdint.h>
#include <types.h>
#include <kernel/apic.h>
#include <kernel/lowlevel.h>
#include <oslib.h>

#define RSDP_SIGNATURE  "RSD PTR "
#define PARAGRAPH_LEN   16

struct RSDPointer {
  char signature[8]; // should be "RSD PTR "
  uint8_t checksum;
  char oemId[6];
  uint8_t revision;
  uint32_t rsdtAddress; // physical address
  uint32_t length;
  uint64_t xsdtAddress; // physical address
  uint8_t extChecksum;
  char _resd[3];
};

_Static_assert(sizeof(struct RSDPointer) == 36, "RSDPointer struct should be 36 bytes.");

struct ACPI_DT_Header {
  char signature[4];
  uint32_t length;
  uint8_t revision;
  uint8_t checksum;
  char oemId[6];
  char oemTableId[8];
  uint32_t oemRevision;
  uint32_t creatorId;
  uint32_t creatorRevision;
};

_Static_assert(sizeof(struct ACPI_DT_Header) == 36, "ACPI_DT_Header should be 36 bytes.");

struct MADT_Header {
  struct ACPI_DT_Header dtHeader;
  uint32_t lapicAddress;
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
  uint8_t lapicId;
  uint32_t flags;
};

#define PROC_LAPIC_ENABLED    1
#define PROC_LAPIC_ONLINE     2

struct IOAPIC_Header {
  struct IC_Header header;
  uint8_t ioapicId;
  uint8_t _resd;
  uint32_t ioapicAddress;
  uint32_t globalSysIntBase;
};

DISC_CODE static bool isValidAcpiHeader(uint64_t physAddress);
DISC_CODE int readAcpiTables(void);

bool isValidAcpiHeader(uint64_t physAddress) {
  struct ACPI_DT_Header header;
  uint8_t checksum = 0;
  uint8_t buffer[128];
  size_t bytesRead = 0;
  size_t headerLen;

  if(physAddress >= MAX_PHYS_MEMORY)
    RET_MSG(false, "Physical memory is out of range.");

  if(IS_ERROR(peek(physAddress, &header, sizeof header)))
    RET_MSG(false, "Unable to read ACPI descriptor table header.");

  headerLen = header.length;

  memcpy(buffer, &header, sizeof header);

  for(size_t i = 0; i < sizeof header; i++, bytesRead++)
    checksum += buffer[i];

  while(bytesRead < headerLen) {
    size_t bytesToRead = MIN(sizeof buffer, headerLen - bytesRead);

    if(IS_ERROR(peek(physAddress + bytesRead, buffer, bytesToRead)))
      RET_MSG(false, "Unable to read ACPI descriptor table header.");

    for(size_t i = 0; i < bytesToRead; i++, bytesRead++)
      checksum += buffer[i];
  }

  return checksum == 0 ? true : false;
}

int readAcpiTables(void) {
  struct RSDPointer rsdp;
  addr_t rsdpAddr;
  size_t processorsFound = 0;

  // Look for the RSDP

  for(uint8_t *ptr = (uint8_t*)KPHYS_TO_VIRT(BIOS_EXT_ROM);
      ptr < (uint8_t*)KPHYS_TO_VIRT(EXTENDED_MEMORY); ptr += PARAGRAPH_LEN)
  {
    if(memcmp(ptr, RSDP_SIGNATURE, sizeof rsdp.signature) == 0) {
      uint8_t checksum = 0;

      for(size_t i = 0; i < offsetof(struct RSDPointer, length); i++)
        checksum += ptr[i];

      if(checksum == 0) {
        rsdp = *(struct RSDPointer*)ptr;
        rsdpAddr = (addr_t)ptr;
        break;
      }
    }
  }

  if(rsdpAddr) {
    // Now go through the RSDT, looking for the MADT

    kprintf("%.8s found at %#x. RSDT at %#x\n", rsdp.signature, rsdpAddr,
            rsdp.rsdtAddress);

    if(rsdp.rsdtAddress && isValidAcpiHeader((uint64_t)rsdp.rsdtAddress)) {
      struct ACPI_DT_Header rsdt;

      if(IS_ERROR(peek((uint64_t )rsdp.rsdtAddress, &rsdt, sizeof rsdt))) {
        kprintf("Unable to read RSDT\n");
        return E_FAIL;
      }

      size_t totalEntries = (rsdt.length - sizeof(struct ACPI_DT_Header))
          / sizeof(uint32_t);

      kprintf("%.4s found at %#x\n", rsdt.signature, rsdp.rsdtAddress);

      for(size_t rsdtIndex = 0; rsdtIndex < totalEntries; rsdtIndex++) {
        uint32_t rsdtEntryPtr;

        if(IS_ERROR(
            peek(
                (uint64_t )rsdp.rsdtAddress + sizeof rsdt
                + rsdtIndex * sizeof(uint32_t),
                &rsdtEntryPtr, sizeof(uint32_t)))) {
          kprintf("Unable to read descriptor table.\n");
          return E_FAIL;
        }

        if(isValidAcpiHeader((uint64_t)rsdtEntryPtr)) {
          struct ACPI_DT_Header header;

          if(IS_ERROR(peek((uint64_t )rsdtEntryPtr, &header, sizeof header))) {
            kprintf("Unable to read descriptor table header.\n");
            continue;
          }

          kprintf("%.4s found at %#x\n", header.signature, rsdtEntryPtr);

          // If the MADT is in the RSDT, then retrieve apic data

          if(memcmp(&header.signature, "APIC", 4) == 0) {
            struct MADT_Header madtHeader;

            if(IS_ERROR(
                peek((uint64_t )rsdtEntryPtr, &madtHeader, sizeof madtHeader)))
            {
              kprintf("Unable to read MADT header.\n");
              return E_FAIL;
            }

            kprintf("Local APIC address: %#x\n", madtHeader.lapicAddress);
            lapicPtr = madtHeader.lapicAddress;

            for(size_t madtOffset = sizeof madtHeader;
                madtOffset < madtHeader.dtHeader.length;)
            {
              struct IC_Header icHeader;

              if(IS_ERROR(
                  peek((uint64_t )rsdtEntryPtr + madtOffset, &icHeader,
                       sizeof icHeader))) {
                kprintf("Unable to read interrupt controller header.\n");
                return E_FAIL;
              }

              switch(icHeader.type) {
                case TYPE_PROC_LAPIC: {
                  /* Each processor has a local APIC. Use this to find each processor's
                   local APIC id. */
                  struct ProcessorLAPIC_Header procLapicHeader;

                  if(IS_ERROR(
                      peek((uint64_t )rsdtEntryPtr + madtOffset,
                           &procLapicHeader, sizeof procLapicHeader))) {
                    kprintf("Unable to read processor local APIC header.\n");
                    return E_FAIL;
                  }

                  if(IS_FLAG_SET(procLapicHeader.flags, PROC_LAPIC_ENABLED) || IS_FLAG_SET(
                      procLapicHeader.flags, PROC_LAPIC_ONLINE)) {
                    kprintf(
                        "Processor %d has local APIC id: %d%s\n",
                        procLapicHeader.uid,
                        procLapicHeader.lapicId,
                        IS_FLAG_SET(procLapicHeader.flags, PROC_LAPIC_ENABLED) ?
                            "" :
                        IS_FLAG_SET(procLapicHeader.flags, PROC_LAPIC_ONLINE) ?
                            " (offline)" : " (disabled)");
                    processors[processorsFound].lapicId = procLapicHeader.lapicId;
                    processors[processorsFound].acpiUid = procLapicHeader.uid;
                    processors[processorsFound].isOnline = !!IS_FLAG_SET(
                        procLapicHeader.flags, PROC_LAPIC_ENABLED);

                    processorsFound++;
                  }
                  break;
                }
                case TYPE_IOAPIC: {
                  struct IOAPIC_Header ioapicHeader;

                  if(IS_ERROR(
                      peek((uint64_t )rsdtEntryPtr + madtOffset, &ioapicHeader,
                           sizeof ioapicHeader))) {
                    kprintf("Unable to read processor local APIC header.\n");
                    return E_FAIL;
                  }

                  kprintf(
                      "IOAPIC id %d is at %#x. Global System Interrupt Base: %#x\n",
                      ioapicHeader.ioapicId, ioapicHeader.ioapicAddress,
                      ioapicHeader.globalSysIntBase);
                  ioapicPtr = ioapicHeader.ioapicId;
                  break;
                }
                default:
                  kprintf("APIC Entry type %d found.\n", icHeader.type);
                  break;
              }

              madtOffset += icHeader.length;
            }

            kprintf("%d processors found.\n", processorsFound);
          }
        }
        else
          kprintf("Unable to read RSDT entries at %#x\n", rsdtEntryPtr);
      }
    }
    else
      kprintf("Unable to read RSDT\n");
  }
  else
    kprintf("RSDP not found\n");

  return processorsFound;
}
