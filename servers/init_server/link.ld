OUTPUT_FORMAT("elf32-i386")
ENTRY(_start)
SECTIONS
{
  virtual = 0x400000;

  .text virtual : ALIGN(4096) {
    exe_code_start = .;
    *(.text*)
    *(.comment)
    *(.rodata*)
    *(.eh_frame)
    *(.debug*)
  }
  
  .data . : ALIGN(4096) {
    exe_data_start = .;
    *(.data*)
    *(.got*)
  }

  .bss . : ALIGN(4096) {
    exe_bss_start = .;
    *(.bss*)
    *(.common*)
  }

  . = ALIGN(0x400000);
  PAGE_MAP_AREA = .;
  GRANT_TABLE = PAGE_MAP_AREA + 0x400000;
  _end = GRANT_TABLE + 0x400000;
}
