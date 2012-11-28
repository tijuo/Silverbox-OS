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
    . = ALIGN(4096);
  }
    __end = .;
}
