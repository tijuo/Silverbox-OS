OUTPUT_FORMAT("elf32-i386")
ENTRY(_start)
SECTIONS
{
  . = 0x400000;

  imageStart = .;

  .text : {
    codeStart = .;
    *(.text)
    *(.comment)
    *(.rodata*)
    codeEnd = .;
  }

  .data : ALIGN(4096) {
    dataStart = .;
    *(.data)
    dataEnd = .;
  }

  .bss : ALIGN(4096) {
    bssStart = .;
    *(.bss)
    *(.common)
    bssEnd = .;
  }

  imageEnd = .;
}
