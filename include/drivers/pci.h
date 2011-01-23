#include <types.h>

#define PCI_BIOS_PHYS                   0xE0000
#define PCI_BIOS_ADDR                   0xE0000

struct PCI_config_header {
  unsigned short vendor_id;
  unsigned short device_id;
  unsigned short command;
  unsigned short status;
  unsigned long	 revision_id : 8;
  unsigned long  class_code  : 24;
  unsigned char  cache_line_sz;
  unsigned char  latency_timer;
  unsigned char  header_type;
  unsigned char  bist;
  unsigned long  base_address_reg[6];
  unsigned long  cardbus_cis_ptr;
  unsigned short subsystem_vendor_id;
  unsigned short subsystem_id;
  unsigned long  rom_base_address;
  unsigned char	 capabilities_ptr;
  unsigned char  reserved[7];
  unsigned char  interrupt_line;
  unsigned char  interrupt_pin;
  unsigned char  min_gnt;
  unsigned char  max_lat;
};

static struct {
  addr_t offset;
  word s;
} pci_indirect = { 0, 0x1B };

typedef struct PCI_device {
  byte bus;
  byte device;
  byte function;
  word class;
  word subclass;
  byte interface;
  struct PCI_config_header config_header;
  struct PCI_device *next, *prev;
} pci_device;

struct {
  pci_device *pci_head;
} system_devices;

typedef struct {
  byte last_bus;
  byte major;
  byte minor;
  byte config1;
  byte config2;
  byte spec_gen1;
  byte spec_gen2;
} pci_hardware;


#define PCI_CONFIG_VENDORID			0x00
#define PCI_CONFIG_DEVICEID			0x02
#define PCI_CONFIG_COMMAND			0x04
#define PCI_CONFIG_STATUS			0x06
#define PCI_CONFIG_REVISIONID			0x08
#define PCI_CONFIG_CLASS			0x09
#define PCI_CONFIG_CACHE			0x0C
#define PCI_CONFIG_LATENCY			0x0D
#define PCI_CONFIG_HEADER_TYPE			0x0E
#define PCI_CONFIG_BIST				0x0F
#define PCI_CONFIG_BASE_ADDR0			0x10
#define PCI_CONFIG_BASE_ADDR1			0x14
#define PCI_CONFIG_BASE_ADDR2			0x18
#define PCI_CONFIG_BASE_ADDR3			0x1B
#define PCI_CONFIG_BASE_ADDR4			0x20
#define PCI_CONFIG_BASE_ADDR5			0x24
#define PCI_CONFIG_CARDBUS_CIS			0x28
#define PCI_CONFIG_SUB_VENDORID			0x2C
#define PCI_CONFIG_SUBSYSTEM_ID			0x2E
#define PCI_CONFIG_EXPANSION_ROM		0x30
#define PCI_CONFIG_IRQ_LINE			0x3C
#define PCI_CONFIG_INTERRUPT_PIN		0x3D
#define PCI_CONFIG_MIN_GRANT			0x3E
#define PCI_MAX_LATENCY				0x3F

#define PCIBIOS_INSTALL_SIGNATURE               0x20494350
#define PCIBIOS_SERVICE                         0x49435024
#define PCIBIOS_SD_MAGIC                        0x5F32335F

/* Functions 0xB181, 0xB182, 0xB183, 0xB186, etc. don't seem to work on
   the machines that I have tested. */

#define PCIBIOS_INSTALL_CHECK                   0xB101
#define PCIBIOS_FIND_DEVICE                     0xB102
#define PCIBIOS_FIND_CLASS                      0xB103
#define PCIBIOS_BUS_OPERATION                   0xB106
#define PCIBIOS_READ_CONFIG_BYTE                0xB108
#define PCIBIOS_READ_CONFIG_WORD                0xB109
#define PCIBIOS_READ_CONFIG_DWORD               0xB10A
#define PCIBIOS_WRITE_CONFIG_BYTE               0xB10B
#define PCIBIOS_WRITE_CONFIG_WORD               0xB10C
#define PCIBIOS_WRITE_CONFIG_DWORD              0xB10D

#define PCI_PRE_PCI_2_0                         0x00
#define PCI_MASS_STORAGE                        0x01
#define PCI_NETWORK_CONTROLLER                  0x02
#define PCI_DISPLAY_CONTROLLER                  0x03
#define PCI_MULTIMEDIA_DEVICE                   0x04
#define PCI_MEMORY_CONTROLLER                   0x05
#define PCI_BRIDGE_DEVICE                       0x06
#define PCI_COMM_CONTROLLER                     0x07
#define PCI_SYSTEM_PERIPHERAL                   0x08
#define PCI_INPUT_DEVICE                        0x09
#define PCI_DOCKING_STATION                     0x0A
#define PCI_PROCESSOR                           0x0B
#define PCI_SERIAL_BUS_CONTROLLER               0x0C

#define bcd_to_bin(num)      ((((num >> 4) & 0xF) * 10) + (num & 0xF))

