OUTPUT_FORMAT("elf32-i386")
ENTRY(_start)
SECTIONS
{
  virtual = 0x1000000;

  .text virtual : {
    __codeStart = .;
    *(.text)
    . = ALIGN(4096);
  }

  .rodata . : {
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
