OUTPUT_FORMAT("elf32-i386")
ENTRY(_start)
SECTIONS
{
  . = 0x400000;

  _begin = .;

  .text : {
    _textStart = .;
    *(.text)
    *(.text.*)
    *(.ctors)
    *(.dtors)
    _textEnd = .;
  }
  
  .rodata : ALIGN(4096) {
    _rodataStart = .;
    *(.rodata)
    *(.rodata.*)
    *(.rdata)
    *(.rdata.*)
    *(.comment)
    *(.comment.*)
    *(.note)
    *(.note.*)
    _rodataEnd = .;
  }

  .data : ALIGN(4096) {
    _dataStart = .;
    *(.data)
    *(.data.*)
    _dataEnd = .;
  }

  .bss : ALIGN(4096) {
    _bssStart = .;
    *(.bss)
    *(.bss.*)
    *(.common)
    *(.common.*)
    _bssEnd = .;
  }
  _end = .;
}
