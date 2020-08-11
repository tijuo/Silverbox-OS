OUTPUT_FORMAT("elf32-i386")
ENTRY(_start)
SECTIONS
{
  virtual = 0x1000000;

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
    . = ALIGN(4096);
  }
  __end = .;
}
