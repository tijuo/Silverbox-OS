/* pci.c
*
* Copyright (C) 2005 Tiju Oliver
*
* This program is free software; you can redistribute it and/or modify it under
* the terms of the GNU General Public License as published by the Free Software
* Foundation; either version 2 of the License, or (at your option) any later
* version.
*
* This program is distributed in the hope that it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
* FOR A PARTICULAR PURPOSE.
*
* For more info, look at the COPYING file.
*/

#include	<types.h>
#include	<oslib.h>
#include	<os/io.h>
#include	<stdio.h>
#include	<string.h>
#include	<os/dev_interface.h>
#include	<os/services.h>
#include	<drivers/pci.h>

static char *class_code0[] = { "Unknown PCI device", "VGA device" };

static char *class_code1[] = {	"SCSI controller", "IDE controller",
				"Floppy disk controller", "IPI controller",
				"RAID controller",
				"Unknown Mass Storage Controller" };

static char *class_code2[] = {	"Ethernet controller",
				"Token ring controller", "FDDI controller",
				"ATM controller",
				"Unknown network controller" };

static char *class_code3[] = {	"VGA controller", "8514 controller",
				"XGA controller",
				"Unknown display controller" };

static char *class_code4[] = {	"Video device", "Audio device",
				"Unknown multimedia device" };

static char *class_code5[] = {	"RAM controller", "Flash memory controller",
				"Unknown memory controller" };

static char *class_code6[] = {	"Host/PCI bridge", "PCI/ISA bridge",
				"PCI/EISA bridge", "PCI/Micro Channel bridge",
				"PCI/PCI bridge", "PCI/PCMCIA bridge",
				"PCI/NuBus bridge", "PCI/CardBus bridge",
				"Unknown bridge type" };

static char *class_code7[] = {	"Generic XT serial controller",
				"16450 serial controller",
				"16550 serial controller",
				"Parallel port",
				"Bi-directional parallel port",
				"ECP 1.X parallel port",
				"Unknown communications device" };

static char *class_code8[] = {	"Generic 8259 programmable interrupt controller",
				"ISA programmable interrupt controller",
				"EISA programmable interrupt controller",
				"Generic 8237 DMA controller",
				"ISA DMA controller", "EISA DMA controller",
				"Generic 8254 timer", "ISA system timer",
				"EISA system timer", "Generic RTC controller",
				"ISA RTC controlller",
				"Unknown system peripheral" };

static char *class_code9[] = {	"Keyboard controller", "Digitizer",
				"Mouse controller",
				"Unknown input controller" };

static char *class_code10[] = { "Generic docking station",
				"Unknown docking station type" };

static char *class_code11[] = { "386 processor",
				"486 processor",
				"Pentium processor",
				"Alpha processor",
				"PowerPC processor",
				"Co-Processor",
				"Unknown processor" };

static char *class_code12[] = { "Firewire (IEEE 1394) controller",
				"ACCESS bus controller", "SSA controller",
				"USB controller",
				"Unknown serial bus controller" };

static char unknown_dev[] = "Unknown Device";

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

far_ptr32 bios32_indirect = { 0, /*0x08*/0x1B };

int (*pci_read_cfg_byte)(byte bus, byte df_num, short reg_num, byte *byte_read);
int (*pci_read_cfg_word)(byte bus, byte df_num, short reg_num, word *word_read);
int (*pci_read_cfg_dword)(byte bus, byte df_num, short reg_num, dword *dword_read);

int (*pci_write_cfg_byte)(byte bus, byte df_num, short reg_num, byte data_to_write);
int (*pci_write_cfg_word)(byte bus, byte df_num, short reg_num, word data_to_write);
int (*pci_write_cfg_dword)(byte bus, byte df_num, short reg_num, dword data_to_write);

static pci_hardware detection_info;

unsigned long bios32_service_call(long service);
//byte detect_pcibios(void);
int bios_pci_detect(pci_hardware *pci_struct);
int bios_read_config_byte(byte bus, byte df_num, short reg_num,
							byte *byte_read);
int bios_read_config_word(byte bus, byte df_num, short reg_num,
							word *word_read);
int bios_read_config_dword(byte bus, byte df_num, short reg_num,
							dword *dword_read);
int find_pci_device(word ven_id, word dev_id, word index,
							pci_device *dev);
int pci_type1_2_scan(void);
int locate_pci_bus(void);
int locate_pci_bios(void);
//void register_pci_device(pci_device device);
//void unregister_pci_device(pci_device device);
char *decode_pci_class(dword class, dword subclass, dword interface);
void pci_scan_devices(void);

int consoleID = -1;

void initPCI(void)
{
/*
  while( consoleID < 0 )
  {
    consoleID = lookupName( "console" );

    if( consoleID < 0 )
      __sleep( 100 );
  }

  if( consoleID < 0 )
    __exit(-1);
*/

  mapMem( (void *)0xB8000, (void *)0xB8000, 8, 0 );
  mapMem( (void *)0xE0000, (void *)0xE0000, 32, 0 );
}

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
  if(err_code == 0x81)
    return 0x81;
  if(err_code != 0)
    return 0xFF;

  return (unsigned)base_addr + offset;
}

byte detect_pcibios(void)
{
  byte           *ptr = (byte *)0xE0000;
  BIOS32         *bios32_sd;
  byte           checksum = 0;
  int            i;

  while((unsigned long)ptr < LOWMEM) {
    bios32_sd = (BIOS32*)ptr;

    if(bios32_sd->magic == 0x5F32335F) {
      for(i=0; i < (bios32_sd->length * 0x10); i++)
        checksum += *(ptr + (byte)i);
      if(checksum == 0){
        if(bios32_sd->rev != 0) {
          printf("Found unsupported PCI BIOS revision 0x%p\n",
                                                bios32_sd->rev);
          continue;
        }
        else {
          bios32_indirect.offset = bios32_sd->entry;
          break;
        }
      }
    }
    else
      ptr += 0x10;
  }

  if((int)ptr >= LOWMEM){
    return 0;
  }

  else {
      return 1;
  }
}

/* BIOS int 0x1A, function 0xB181. Checks to see if the PCI 
   bus is present.*/

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

int mech1_pci_read_config_byte(byte bus, byte df_num, short reg_num, byte *byte_read)
{
  dword data;

  data = 0x80000000;
  data |= (bus << 16);
  data |= (df_num << 8);
  data |= (reg_num & 0xFFFFFFFC);
  outDword(0xCF8, data);
  *byte_read = inByte(0xCFC + (reg_num & 3));

  return 0;
}

int mech1_pci_read_config_word(byte bus, byte df_num, short reg_num, word *word_read)
{
  dword data;

  data = 0x80000000;
  data |= (bus << 16);
  data |= (df_num << 8);
  data |= (reg_num & 0xFFFFFFFC);
  outDword(0xCF8, data);
  *word_read = inWord(0xCFC + (reg_num & 2));

  return 0;
}

int mech1_pci_read_config_dword(byte bus, byte df_num, short reg_num, dword *dword_read)
{
  dword data;

  data = 0x80000000;
  data |= (bus << 16);
  data |= (df_num << 8);
  data |= (reg_num & 0xFFFFFFFC);
  outDword(0xCF8, data);
  *dword_read = inDword(0xCFC + reg_num);

  return 0;
}

int mech1_pci_write_config_byte(byte bus, byte df_num, short reg_num, byte data_to_write)
{
  dword data;

  data = 0x80000000;
  data |= (bus << 16);
  data |= (df_num << 8);
  data |= (reg_num & 0xFFFFFFFC);
  outDword(0xCF8, data);
  outByte(0xCFC + (reg_num & 3), data_to_write);

  return 0;
}

int mech1_pci_write_config_word(byte bus, byte df_num, short reg_num, word data_to_write)
{
  dword data;

  data = 0x80000000;
  data |= (bus << 16);
  data |= (df_num << 8);
  data |= (reg_num & 0xFFFFFFFC);
  outDword(0xCF8, data);
  outWord(0xCFC + (reg_num & 2), data_to_write);

  return 0;
}

int mech1_pci_write_config_dword(byte bus, byte df_num, short reg_num, dword data_to_write)
{
  dword data;

  data = 0x80000000;
  data |= (bus << 16);
  data |= (df_num << 8);
  data |= (reg_num & 0xFFFFFFFC);
  outDword(0xCF8, data);
  outDword(0xCFC + reg_num, data_to_write);

  return 0;
}


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

/* Attempts to locate PCI BIOS by using the BIOS Service Directory */

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

/* Attemps to locate the PCI bus by either trying to find PCI BIOS or using
   configuration space access mechanism 1 or 2 */

int locate_pci_bus(void)
{
  int type;

  if(locate_pci_bios() == 0)
  {
    printf("Can't locate PCI BIOS\n");

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
  }
  else {

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

/*
    print("Last Bus: ");
    printInt( detection_info.last_bus );
    print(" Revision Major: ");
    printInt( detection_info.major );
    print(" Minor: ");
    printInt( detection_info.minor );
    print("\nPCI Configuration Space Access Type 1: ");
    print( detection_info.config1 ? "yes" : "no" );
    print("\nPCI Configuration Space Access Type 2: ");
    print( detection_info.config2 ? "yes" : "no" );
    print("\nPCI Special Cycle Generation 1: ");
    print( detection_info.spec_gen1 ? "yes" : "no" );
    print("\nPCI Special Cycle Generation 2: ");
    print( detection_info.spec_gen2 ? "yes" : "no" );
    print("\n");
*/
  }

  return 1;
}

char *decode_pci_class(dword class, dword subclass, dword interface)
{
  switch(class){
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
  pci_device dev;
  byte multifunc;
  word data;
  dword class;
  char *string;

  system_devices.pci_head = NULL;

  for(dev.bus = 0; dev.bus <= detection_info.last_bus; dev.bus++){
    for(dev.device = 0; dev.device < 32; dev.device++) {
      dev.function = 0;
      pci_read_cfg_byte(dev.bus, (dev.device << 3) | (dev.function), 0xE, &multifunc);
      multifunc &= 0x80;
      multifunc >>= 7;

      while(dev.function < 8) {
        pci_read_cfg_word(dev.bus, (dev.device << 3) | (dev.function), PCI_CONFIG_VENDORID, &data);
        if(data == 0xFFFF)
          break;
        else 
        {
          printf("Vendor ID: 0x%x ", data);
	  dev.config_header.vendor_id = data;
          pci_read_cfg_word(dev.bus, (dev.device << 3) | (dev.function), PCI_CONFIG_DEVICEID, &data);
	  dev.config_header.device_id = data;

          printf(" Device ID: 0x%x ", data);
          printf(" Bus: %d Device: %d Function %d\n", dev.bus, dev.device, dev.function);

          pci_read_cfg_dword(dev.bus, (dev.device << 3) | (dev.function), 8, &class);
          class &= 0xFFFFFF;
	  dev.config_header.class_code = class;
	  dev.class = class >> 16;
          dev.subclass = (class & 0xFF00) >> 8;
          dev.interface = class & 0xFF;
          string = decode_pci_class(class >> 16, ((class & 0xFF00) >> 8), class & 0xFF);

	  printf("%s\n", string);

	 /* register_pci_device(dev); */
	  if(multifunc)
            dev.function++;
	  else
            break;
        }
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

