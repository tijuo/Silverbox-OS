OUTPUT_FORMAT("elf32-i386")
ENTRY(start)
SECTIONS
{
  __virtual = 0x400000;

  .text __virtual : {
    __code = .;
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
    __data = .;
    *(.rdata)
    *(.data)
    *(.gnu.linkonce.d.*)
    . = ALIGN(4096);
  }

  .bss . : {
    __bss = .;
    *(.bss)
    *(.gnu.linkonce.b.*)
    *(.common)
    . = ALIGN(4096);
  }
  __end = .;
}
