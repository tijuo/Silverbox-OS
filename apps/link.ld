OUTPUT_FORMAT("elf64-x86-64")
ENTRY(_start)
SECTIONS
{
  .text 0x8000000000 : ALIGN(4K) {
     elf64_image_start = .;
     segment_text_start = .;
     *(.text*)
     *(.interp*)
     . = ALIGN(4K);
     segment_text_end = .;
  }

  .rodata : ALIGN(4K) {
    *(.rodata*)
    *(.rela*)
  }

  .data : ALIGN(4K) {
     segment_data_start = .;

     *(.data*)
     *(.dynamic*)
     *(.dynstr*)
     *(.dynsym*)
     *(.got*)
     *(.rel*)
     *(.shstrtab*)
     *(.strtab*)
     *(.symtab*)
     *(.hash*)
     . = ALIGN(4K);
     segment_data_end = .;
  }

  .bss : ALIGN(4K) {
    segment_bss_start = .;
    *(.bss*)
    *(.COMMON)
     . = ALIGN(4K);
    segment_bss_end = .;
  }

  elf64_image_end = .;

  /DISCARD/ : {
    *(.comment*)
    *(.note*)
  }
}
