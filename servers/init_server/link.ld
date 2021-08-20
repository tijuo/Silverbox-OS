OUTPUT_FORMAT("elf32-i386")
ENTRY(_start)
SECTIONS
{
  virtual = 0x1000000;

  .text virtual : {
    _code_start = .;
    *(.text*)
    *(.comment)
    *(.rodata*)
    *(.eh_frame)
    . = ALIGN(4096);
  }
  
  .data . : {
    _data_start = .;
    *(.data*)
    *(.got*)
    . = ALIGN(4096);
  }

  .bss . : {
    _bss_start = .;
    *(.bss*)
    *(.common*)
    . = ALIGN(4096);
  }

  PAGE_MAP_AREA = .;
  _end = PAGE_MAP_AREA + 0x400000;
}
