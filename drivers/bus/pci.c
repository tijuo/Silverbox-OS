#include	<oslib.h>
#include	<os/io.h>
#include	<stdio.h>
#include	<string.h>
#include	<os/dev_interface.h>
#include	<os/services.h>
#include	<drivers/pci.h>

#include "../../include/type.h"
/*
char *class_codes[][] = {
{ "Unknown PCI device", "VGA device" },

{  "SCSI controller", "IDE controller", "Floppy disk controller",
   "IPI controller", "RAID controller", "Unknown Mass Storage Controller" },

{  "Ethernet controller", "Token ring controller", "FDDI controller",
   "ATM controller", "Unknown network controller" },

{  "VGA controller", "8514 controller", "XGA controller",
   "Unknown display controller" },

{  "Video device", "Audio device", "Unknown multimedia device" },

{  "RAM controller", "Flash memory controller", "Unknown memory controller" },

{  "Host/PCI bridge", "PCI/ISA bridge", "PCI/EISA bridge",
   "PCI/Micro Channel bridge", "PCI/PCI bridge", "PCI/PCMCIA bridge",
   "PCI/NuBus bridge", "PCI/CardBus bridge", "Unknown bridge type" },

{  "Generic XT serial controller", "16450 serial controller",
   "16550 serial controller", "Parallel port", "Bi-directional parallel port",
   "ECP 1.X parallel port", "Unknown communications device" },

{  "Generic 8259 programmable interrupt controller",
   "ISA programmable interrupt controller",
   "EISA programmable interrupt controller",
   "Generic 8237 DMA controller", "ISA DMA controller", "EISA DMA controller",
   "Generic 8254 timer", "ISA system timer", "EISA system timer",
   "Generic RTC controller", "ISA RTC controlller",
   "Unknown system peripheral" },

{  "Keyboard controller", "Digitizer", "Mouse controller",
   "Unknown input controller" },

{ "Generic docking station", "Unknown docking station type" },

{ "386 processor", "486 processor", "Pentium processor", "Alpha processor",
  "PowerPC processor", "Co-Processor", "Unknown processor" },

{ "Firewire (IEEE 1394) controller", "ACCESS bus controller", "SSA controller",
  "USB controller", "Unknown serial bus controller" }
};
*/


const char *class_code0[] = { "Unknown PCI device", "VGA device" };

const char *class_code1[] = {	"SCSI controller", "IDE controller",
				"Floppy disk controller", "IPI controller",
				"RAID controller",
				"Unknown Mass Storage Controller" };

const char *class_code2[] = {	"Ethernet controller",
				"Token ring controller", "FDDI controller",
				"ATM controller",
				"Unknown network controller" };

const char *class_code3[] = {	"VGA controller", "8514 controller",
				"XGA controller",
				"Unknown display controller" };

const char *class_code4[] = {	"Video device", "Audio device",
				"Unknown multimedia device" };

const char *class_code5[] = {	"RAM controller", "Flash memory controller",
				"Unknown memory controller" };

const char *class_code6[] = {	"Host/PCI bridge", "PCI/ISA bridge",
				"PCI/EISA bridge", "PCI/Micro Channel bridge",
				"PCI/PCI bridge", "PCI/PCMCIA bridge",
				"PCI/NuBus bridge", "PCI/CardBus bridge",
				"Unknown bridge type" };

const char *class_code7[] = {	"Generic XT serial controller",
				"16450 serial controller",
				"16550 serial controller",
				"Parallel port",
				"Bi-directional parallel port",
				"ECP 1.X parallel port",
				"Unknown communications device" };

const char *class_code8[] = {	"Generic 8259 programmable interrupt controller",
				"ISA programmable interrupt controller",
				"EISA programmable interrupt controller",
				"Generic 8237 DMA controller",
				"ISA DMA controller", "EISA DMA controller",
				"Generic 8254 timer", "ISA system timer",
				"EISA system timer", "Generic RTC controller",
				"ISA RTC controlller",
				"Unknown system peripheral" };

const char *class_code9[] = {	"Keyboard controller", "Digitizer",
				"Mouse controller",
				"Unknown input controller" };

const char *class_code10[] = { "Generic docking station",
				"Unknown docking station type" };

const char *class_code11[] = { "386 processor",
				"486 processor",
				"Pentium processor",
				"Alpha processor",
				"PowerPC processor",
				"Co-Processor",
				"Unknown processor" };

const char *class_code12[] = { "Firewire (IEEE 1394) controller",
				"ACCESS bus controller", "SSA controller",
				"USB controller",
				"Unknown serial bus controller" };

const char *unknown_dev = "Unknown Device";

/*
typedef struct {
        unsigned long   magic;
        unsigned long   entry;
        byte            rev;
        byte            length;
        byte            checksum;
        byte            _resd[5];       // Reserved
} BIOS32;

typedef struct {
  dword offset;
  word  descriptor;
} far_ptr32;

far_ptr32 bios32_indirect = { 0, 0x1B };
*/

int (*pci_read_cfg_byte)(byte bus, byte dev, byte func, byte offset, byte *byte_read);
int (*pci_read_cfg_word)(byte bus, byte dev, byte func, byte offset, word *word_read);
int (*pci_read_cfg_dword)(byte bus, byte dev, byte func, byte offset, dword *dword_read);

int (*pci_write_cfg_byte)(byte bus, byte dev, byte func, byte offset, byte data_to_write);
int (*pci_write_cfg_word)(byte bus, byte dev, byte func, byte offset, word data_to_write);
int (*pci_write_cfg_dword)(byte bus, byte dev, byte func, byte offset, dword data_to_write);

//static pci_hardware detection_info;

//unsigned long bios32_service_call(long service);
//byte detect_pcibios(void);
/*
int bios_pci_detect(pci_hardware *pci_struct);
int bios_read_config_byte(byte bus, byte df_num, short reg_num,
							byte *byte_read);
int bios_read_config_word(byte bus, byte df_num, short reg_num,
							word *word_read);
int bios_read_config_dword(byte bus, byte df_num, short reg_num,
							dword *dword_read);
*/
int find_pci_device(word ven_id, word dev_id, word index,
							pci_device *dev);
int pci_type1_2_scan(void);
int locate_pci_bus(void);
//int locate_pci_bios(void);
//void register_pci_device(pci_device device);
//void unregister_pci_device(pci_device device);
const char *decode_pci_class(dword pci_class, dword subclass, dword interface);
void pci_scan_devices(void);

void initPCI(void)
{
//  mapMem( (void *)PCI_BIOS_PHYS, (void *)PCI_BIOS_ADDR, 32, 0 );
  changeIoPerm( PCI_CFG_ADDR, PCI_CFG_ADDR + 7, 1 );
}

/*
unsigned long bios32_service_call(long service)
{
  byte  err_code;
  addr_t base_addr;
  unsigned long size;
  unsigned long offset;

  asm("lcall   *(%%edi)\n\t" \
        : "=a"(err_code),\
        "=b"(base_addr), \
        "=d" (offset), \
        "=c" (size) \
        : "0"(service), "1"(0), "D"(&bios32_indirect));

  if(err_code == 0x80)
    return 0x80;
  else if(err_code == 0x81)
    return 0x81;
  else if(err_code != 0)
    return 0xFF;
  else
    return (unsigned)base_addr + offset;
}

byte detect_pcibios(void)
{
  byte           *ptr = (byte *)PCI_BIOS_ADDR;
  BIOS32         *bios32_sd;
  byte           checksum = 0;
  int            i;

  printf("Scanning for PCI BIOS...\n");

  while((unsigned long)ptr < ((unsigned long)PCI_BIOS_ADDR + 32 * 4096)) {
    bios32_sd = (BIOS32*)ptr;

    if(bios32_sd->magic == 0x5F32335F)
    {
      for(i=0; i < (bios32_sd->length * 0x10); i++)
        checksum += *(ptr + (byte)i);

      if(checksum == 0)
      {
        if(bios32_sd->rev != 0)
        {
          printf("Found unsupported PCI BIOS revision 0x%p\n",
                                                bios32_sd->rev);
          continue;
        }
        else
        {
          bios32_indirect.offset = bios32_sd->entry;
          break;
        }
      }
    }
    else
      ptr += 0x10;
  }

  if((int)ptr >= ((unsigned long)PCI_BIOS_ADDR + 32 * 4096))
    return 0;
  else
    return 1;
}
*/

/* BIOS int 0x1A, function 0xB181. Checks to see if the PCI 
   bus is present.*/

/*
int bios_pci_detect(pci_hardware *pci_struct)
{
  byte bx, hwchar;
  dword pci_sig;

  asm("lcall	*(%%esi)\n\t" \
	: "=a"(hwchar), \
	"=b"(bx), \
	"=c"(pci_struct->last_bus), \
	"=d"(pci_sig) \
	: "0"(PCIBIOS_INSTALL_CHECK), \
	"D"(0x0), \
	"S"(&pci_indirect));

  if(pci_sig != PCIBIOS_INSTALL_SIGNATURE)
    return 0;

  pci_struct->minor = bcd_to_bin((bx & 0xFF));
  pci_struct->major = bcd_to_bin((bx >> 8));

  pci_struct->config1 = hwchar & 0x1;
  pci_struct->config2 = (hwchar & 0x2) >> 1;
  pci_struct->spec_gen1 = (hwchar & 0x10) >> 4;
  pci_struct->spec_gen2 = (hwchar & 0x20) >> 5;

  return 1;
}

int bios_read_config_byte(byte bus, byte df_num, short reg_num, byte *byte_read)
{
  dword bx;
  byte status;

  bx = bus << 8;
  bx |= df_num;

  asm("lcall	*(%%esi)\n\t" \
	: "=a"(status), \
	"=c"(*byte_read) \
	: "0"(PCIBIOS_READ_CONFIG_BYTE), \
	"b"(bx), \
	"D"(reg_num), \
	"S"(&pci_indirect));

  if(status != 0)
    return 1;
  else
    return 0;
}

int bios_read_config_word(byte bus, byte df_num, short reg_num, word *word_read)
{
  dword bx;
  byte status;

  bx = bus << 8;
  bx |= df_num;

  asm("lcall    *(%%esi)\n\t" \
        : "=a"(status), \
        "=c"(*word_read) \
        : "0"(PCIBIOS_READ_CONFIG_WORD), \
        "b"(bx), \
        "D"(reg_num), \
        "S"(&pci_indirect));

  if(status != 0)
    return 1;
  else
    return 0;
}

int bios_read_config_dword(byte bus, byte df_num, short reg_num, dword *dword_read)
{
  dword bx;
  byte status;

  bx = bus << 8;
  bx |= df_num;

  asm("lcall    *(%%esi)\n\t" \
        : "=a"(status), \
        "=c"(*dword_read) \
        : "0"(PCIBIOS_READ_CONFIG_DWORD), \
        "b"(bx), \
        "D"(reg_num), \
        "S"(&pci_indirect));

  if(status != 0)
    return 1;
  else
    return 0;
}

int bios_write_config_byte(byte bus, byte df_num, short reg_num, byte data_to_write)
{
  dword bx;
  byte status;

  bx = bus << 8;
  bx |= df_num;

  asm("lcall    *(%%esi)\n\t" \
        : "=a"(status) \
        : "0"(PCIBIOS_WRITE_CONFIG_BYTE), \
        "b"(bx), \
	"c"(data_to_write), \
        "D"(reg_num), \
        "S"(&pci_indirect));

  if(status != 0)
    return 1;
  else
    return 0;
}

int bios_write_config_word(byte bus, byte df_num, short reg_num, word data_to_write)
{
  dword bx;
  byte status;

  bx = bus << 8;
  bx |= df_num;

  asm("lcall    *(%%esi)\n\t" \
        : "=a"(status) \
        : "0"(PCIBIOS_WRITE_CONFIG_WORD), \
        "b"(bx), \
	"c"(data_to_write), \
        "D"(reg_num), \
        "S"(&pci_indirect));

  if(status != 0)
    return 1;
  else
    return 0;
}

int bios_write_config_dword(byte bus, byte df_num, short reg_num, dword data_to_write)
{
  dword bx;
  byte status;

  bx = bus << 8;
  bx |= df_num;

  asm("lcall    *(%%esi)\n\t" \
        : "=a"(status) \
        : "0"(PCIBIOS_WRITE_CONFIG_DWORD), \
        "b"(bx), \
	"c"(data_to_write), \
        "D"(reg_num), \
        "S"(&pci_indirect));

  if(status != 0)
    return 1;
  else
    return 0;
}
*/
// Includes enable bit at most significant bit

#define PCI_ADDR(bus,dev,func,off)	((bus << 16) | ((dev & 0x1f) << 11) \
					| ((func & 7) << 8) | (off & 0x3f) \
					| (1 << 31))

int mech1_pci_read_config_byte(byte bus, byte dev, byte func, byte offset, byte *byte_read)
{
  outDword(PCI_CFG_ADDR, PCI_ADDR(bus,dev,func,offset));
  *byte_read = inByte(PCI_CFG_DATA + (offset % 4));

  return 0;
}

int mech1_pci_read_config_word(byte bus, byte dev, byte func, byte offset, word *word_read)
{
  outDword(PCI_CFG_ADDR, PCI_ADDR(bus,dev,func,offset));
  *word_read = inWord(PCI_CFG_DATA + (offset % 4));

  return 0;
}

int mech1_pci_read_config_dword(byte bus, byte dev, byte func, byte offset, dword *dword_read)
{
  outDword(PCI_CFG_ADDR, PCI_ADDR(bus,dev,func,offset));
  *dword_read = inDword(PCI_CFG_DATA);

  return 0;
}

int mech1_pci_write_config_byte(byte bus, byte dev, byte func, byte offset, byte data_to_write)
{
  outDword(PCI_CFG_ADDR, PCI_ADDR(bus,dev,func,offset));
  outByte(PCI_CFG_DATA + (offset % 4), data_to_write);

  return 0;
}

int mech1_pci_write_config_word(byte bus, byte dev, byte func, byte offset, word data_to_write)
{
  outDword(PCI_CFG_ADDR, PCI_ADDR(bus,dev,func,offset));
  outWord(PCI_CFG_DATA + (offset % 4), data_to_write);

  return 0;
}

int mech1_pci_write_config_dword(byte bus, byte dev, byte func, byte offset, dword data_to_write)
{
  outDword(PCI_CFG_ADDR, PCI_ADDR(bus,dev,func,offset));
  outDword(PCI_CFG_DATA, data_to_write);

  return 0;
}

/*
int find_pci_device(word ven_id, word dev_id, word index, pci_device *dev)
{
  word bx;

  unsigned status;

  asm("lcall    *(%%edi)\n\t" \
        : "=a"(status), \
	"=b"(bx) \
        : "0"(PCIBIOS_FIND_DEVICE), \
        "S"(index), \
        "c"(dev_id), \
        "d"(ven_id), \
        "D"(&pci_indirect));

  status = (status & 0xFFFF) >> 8;

  if(status != 0)
    return status;

  dev->bus = bx >> 8;
  dev->device = ((bx & 0xF8) >> 3);
  dev->function = bx & 0x7;

  return 0;
}
*/
/* Attempts to locate PCI BIOS by using the BIOS Service Directory */

/*
int locate_pci_bios(void)
{
  if(detect_pcibios() == 1)
  {
    pci_indirect.offset = (addr_t)bios32_service_call(PCIBIOS_SERVICE);

    if((unsigned)pci_indirect.offset > 0x100)
      return bios_pci_detect(&detection_info);
    else
      return 0;
  }
  else
    return 0;
}
*/

int pci_type1_2_scan(void)
{
  dword t;

  outByte(0xCFB, 0x01);
  t = inDword(0xCF8);
  outDword( 0xCF8, 0x80000000 );

  if (inDword(0xCF8) == 0x80000000)
  {
    outDword(0xCF8, t);
    return 1;
  }

  outDword(0xCF8, t);

  outByte(0xCFB, 0x00);
  outByte(0xCF8, 0x00);
  outByte(0xCFA, 0x00);

  if(inByte(0xCF8) == 0x00 && inByte(0xCFA) == 0x00)
    return 2;

  return 0;
}

/* Attemps to locate the PCI bus by using configuration space access mechanism 1 or 2 */

int locate_pci_bus(void)
{
  int type;
/*
  if(locate_pci_bios() == 0)
  {
    printf("Can't locate PCI BIOS\n");
*/
    if((type = pci_type1_2_scan()) == 0)
    {
      printf("PCI bus not present\n");
      return 0;
    }
    else if(type == 1) {
      pci_read_cfg_byte = &mech1_pci_read_config_byte;
      pci_read_cfg_word = &mech1_pci_read_config_word;
      pci_read_cfg_dword = &mech1_pci_read_config_dword;

      pci_write_cfg_byte = &mech1_pci_write_config_byte;
      pci_write_cfg_word = &mech1_pci_write_config_word;
      pci_write_cfg_dword = &mech1_pci_write_config_dword;
    }
    else if(type == 2) {
/*
      pci_cfg_read_byte = &mech2_pci_config_read_byte;
      pci_cfg_read_word = &mech2_pci_config_read_word;
      pci_cfg_read_dword = &mech2_pci_config_read_dword;

      pci_cfg_write_byte = &mech2_pci_config_write_byte;
      pci_cfg_write_word = &mech2_pci_config_write_word;
      pci_cfg_write_dword = &mech2_pci_config_write_dword;
*/
    printf("No support for PCI Configuration Space Access type 2(yet)\n");
    }
/*  }

  else
  {

      pci_read_cfg_byte = &bios_read_config_byte;
      pci_read_cfg_word = &bios_read_config_word;
      pci_read_cfg_dword = &bios_read_config_dword;

      pci_write_cfg_byte = &bios_write_config_byte;
      pci_write_cfg_word = &bios_write_config_word;
      pci_write_cfg_dword = &bios_write_config_dword;

    printf("Last Bus: %d Revision Major: %d Minor: %d\n", detection_info.last_bus, \
	detection_info.major, detection_info.minor);
    printf("PCI Configuration Space Access Type 1: %s\n", detection_info.config1 ? "yes" : "no");
    printf("PCI Configuration Space Access Type 2: %s\n", detection_info.config2 ? "yes" : "no");
    printf("PCI Special Cycle Generation 1: %s\n", detection_info.spec_gen1 ? "yes" : "no");
    printf("PCI Special Cycle Generation 2: %s\n", detection_info.spec_gen2 ? "yes" : "no");
  }
*/
  return 1;
}

const char *decode_pci_class(dword pci_class, dword subclass, dword interface)
{
  switch(pci_class){
    case PCI_PRE_PCI_2_0:
      if(subclass == 0x1 && interface == 0x1)
        return class_code0[1];
      else
        return class_code0[0];
      break;

    case PCI_MASS_STORAGE:
      if(subclass > 0x4)
        return class_code1[5];
      else
        return class_code1[subclass];
      break;

    case PCI_NETWORK_CONTROLLER:
      if(subclass > 0x03)
        return class_code2[4];
      else
        return class_code2[subclass];
      break;

    case PCI_DISPLAY_CONTROLLER:
      if(subclass > 0x01 || interface > 0x01)
        return class_code3[3];
      else if(subclass == 0)
        return class_code3[interface];
      else
        return class_code3[2];
      break;

    case PCI_MULTIMEDIA_DEVICE:
      if(subclass > 0x01)
        return class_code4[2];
      else
        return class_code4[subclass];
      break;

    case PCI_MEMORY_CONTROLLER:
      if(subclass > 0x01)
        return class_code5[2];
      else
        return class_code5[subclass];
      break;

    case PCI_BRIDGE_DEVICE:
      if(subclass > 0x07)
        return class_code6[8];
      else
        return class_code6[subclass];
      break;

    case PCI_COMM_CONTROLLER:
      if(subclass > 0x01 || interface > 0x02)
        return class_code7[6];
      else if(subclass == 0)
        return class_code7[interface];
      else if(subclass == 1)
        return class_code7[interface + 3];
      break;

    case PCI_SYSTEM_PERIPHERAL:
      if(subclass > 0x03 || interface > 0x02)
        return class_code8[11];
      else if(subclass == 0)
        return class_code8[interface];
      else if(subclass == 1)
        return class_code8[interface + 3];
      else if(subclass == 2)
        return class_code8[interface + 6];
      else if(subclass == 3) {
        if(interface > 0x01)
          return class_code8[11];
        else
          return class_code8[interface + 9];
      }
      break;

    case PCI_INPUT_DEVICE:
      if(subclass > 0x02)
        return class_code9[3];
      else
        return class_code9[subclass];
      break;

    case PCI_DOCKING_STATION:
      if(subclass != 0)
        return class_code10[1];
      else
        return class_code10[0];
      break;

    case PCI_PROCESSOR:
      if(subclass == 0)
        return class_code11[0];
      else if(subclass == 0x01)
        return class_code11[1];
      else if(subclass == 0x02)
        return class_code11[2];
      else if(subclass == 0x10)
        return class_code11[3];
      else if(subclass == 0x20)
        return class_code11[4];
      else if(subclass == 0x40)
        return class_code11[5];
      else
        return class_code11[6];
      break;
    case PCI_SERIAL_BUS_CONTROLLER:
      if(subclass > 0x03)
        return class_code12[4];
      else
        return class_code12[subclass];
      break;
    default:
      return unknown_dev;
      break;
  }
  return unknown_dev;
}
/*
void register_pci_device(pci_device device)
{
  pci_device *newdev, *c;

  newdev = (pci_device *)malloc(sizeof(pci_device));

  *newdev = device;

  newdev->prev = NULL;
  newdev->next = NULL;

  if(system_devices.pci_head == NULL)
    system_devices.pci_head = newdev;
  else {
    for(c = system_devices.pci_head; c->next != NULL; c = c->next);
    c->next = newdev;
    newdev->prev = c;
  }
}

void unregister_pci_device(pci_device device)
{
  pci_device *c;

  for(c = system_devices.pci_head; c->next != NULL; c = c->next);
  c->prev->next = c->next;
  c->next->prev = c->prev;
  kfree(c);
}
*/

void pci_scan_devices(void)
{
  int bus, device, func;
  pci_device dev;
  byte multifunc;
  dword data;
  const char *string;

  system_devices.pci_head = NULL;

  printf("Scannng for PCI devices...\n");

  for(bus = 0; bus <= 255/*detection_info.last_bus*/; bus++)
  {
    for(device = 0; device < 32; device++)
    {
      pci_read_cfg_dword(bus, device, 0, 0, &data);

      if( data == 0xFFFFFFFF )
      {
        if( device == 0 )
          break;
        else
          continue;
      }

      pci_read_cfg_byte(bus, device, 0, 0xE, &multifunc);

      for( func=0; func < 8; func++ )
      {
        pci_read_cfg_dword(bus, device, func, 0,
          ((dword *)&dev.config_header));

        if( dev.config_header.vendor_id == 0xFFFF &&
            dev.config_header.device_id == 0xFFFF )
        {
          continue;
        }

        for( int i=1; i < 16; i++ )
        {
          pci_read_cfg_dword(bus, device, func, i << 2,
            ((dword *)&dev.config_header) + i);
        }

        dev.bus = (byte)bus;
        dev.device = (byte)device;
        dev.function = (byte)func;

        dev.class = dev.config_header.class_code >> 16;
        dev.subclass = (dev.config_header.class_code >> 8) & 0xFF;
        dev.interface = dev.config_header.class_code & 0xFF;
        string = decode_pci_class(dev.class, dev.subclass, dev.interface);

        printf("%s - <0x%x:0x%x> @ (%d:%d.%d)\n", string, dev.config_header.vendor_id,
          dev.config_header.device_id, bus, device, func);

	if(!(multifunc & 0x80))
          break;
      }
    }
  }

  printf("PCI bus scan complete.\n");
}

int main(void)
{
  initPCI();

  if(locate_pci_bus() == 1)
    pci_scan_devices();

  return 0;
}

