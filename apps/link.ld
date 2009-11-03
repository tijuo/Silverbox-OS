/* link.ld
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

OUTPUT_FORMAT("elf32-i386")
ENTRY(_start)
SECTIONS
{
  virtual = 0x400000;

  .text virtual : {
    __codeStart = .;
    *(.text)
    *(.comment)
    *(.rodata*)
    . = ALIGN(4096);
  }
  
  .data . : {
    __dataStart = .;
    *(.rdata)
    *(.data)
    . = ALIGN(4096);
  }

  .bss . : {
    __bssStart = .;
    *(.bss)
    *(.common)
  }
    __end = .;
}
