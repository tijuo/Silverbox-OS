OUTPUT_FORMAT("elf32-i386")
ENTRY(start)
SECTIONS
{
  virtual = 0x400000;

  .text virtual : {
    code = .;
    *(.text)
    *(.gnu.linkonce.t.*)
    *(.gnu.linkonce.r.*)

    . = ALIGN(4);

    start_ctors = .;
    *(.ctor*)
    end_ctors = .;

    . = ALIGN(4);
    start_dtors = .;
    *(.dtor*) 
    end_dtors = .;

    *(.comment)
    *(.rodata*)
    . = ALIGN(4096);
  }
  
  .data . : {
    data = .;
    *(.rdata)
    *(.data)
    *(.gnu.linkonce.d.*)
    . = ALIGN(4096);
  }

  .bss . : {
    bss = .;
    *(.bss)
    *(.gnu.linkonce.b.*)
    *(.common)
    . = ALIGN(4096);
  }
    end = .;
}
