/* link.ld
*
* Copyright (C) 2006 Tiju Oliver
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

OUTPUT_FORMAT("elf32-i386")
ENTRY(start)
SECTIONS
{
  kPhysCode  = 0x1000000;
  kPhysStart = 0x1000000;
  kVirtCode  = 0xFF400000;
  kVirtStart = 0xFF400000;

  kVirtToPhys = kPhysStart - kVirtStart;
  kPhysToVirt = kVirtStart - kPhysStart;
  VPhysMemStart = 0x00000000;

  kernelIDT = VPhysMemStart + 0x88000;
  kernelGDT = 0x88800 + VPhysMemStart;
  kernelTSS = 0x88F98 + VPhysMemStart;

  .text kVirtCode : AT(kPhysCode) {
     kCode = .;
     *(.header)
     *(.text*)
     *(.rodata*)
     *(.comment)
     . = ALIGN(4096);
  }
  
  kPhysData = . + kVirtToPhys;
  kVirtData = .;

  .data . : AT(kPhysData) {
     kData = .;
     *(.data*)
     . = ALIGN(4096);
  }

  kPhysDisc = . + kVirtToPhys;
  kVirtDisc = .;

  .disc . : AT(kPhysDisc) {
    kdCode = .;
    *(.dtext)
    kdData = .;
    *(.ddata)
    . = ALIGN(4096);
  }

  kPhysBss = . + kVirtToPhys;
  kVirtBss = .;

  .bss kVirtBss : AT(kPhysBss) {
     kBss = .;
     *(.bss)
/*     . = ALIGN(4096); */
  }
  
  kEnd = .;
  kSize = kEnd - kCode;
}
