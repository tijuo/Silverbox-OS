/* cLink.ld
*
* Copyright (C) 2007 Tiju Oliver
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
  virtual = 0x100000;

  .text virtual : {
    __code = .;
    *(.text)
    *(.comment)
    *(.rodata*)
    . = ALIGN(4096);
  }
  
  .data . : {
    __data = .;
    *(.rdata)
    *(.data)
    . = ALIGN(4096);
  }

  .bss . : {
    __bss = .;
    *(.bss)
    *(.common)
  }
    __end = .;
}
