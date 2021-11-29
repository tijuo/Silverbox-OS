OUTPUT_FORMAT("elf32-i386")
ENTRY(_start)
SECTIONS
{
  virtual = 0x400000;

  .text virtual : ALIGN(4K) {
    exe_code_start = .;
    *(.text*)
    *(.comment)
    *(.rodata*)
    *(.eh_frame)
    *(.debug*)
  }
  
  .data . : ALIGN(4K) {
    exe_data_start = .;
    *(.data*)
    *(.got*)
  }

  .bss . : ALIGN(4K) {
    exe_bss_start = .;
    *(.bss*)
    *(.common*)
  }

  . = ALIGN(4M);
  PAGE_MAP_AREA = .;
  GRANT_TABLE = PAGE_MAP_AREA + 4M;
  _end = GRANT_TABLE + 4M;
}
