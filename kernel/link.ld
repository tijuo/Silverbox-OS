PHDRS {
    boot PT_LOAD FLAGS(7);
    text PT_LOAD FLAGS(5);
    rodata PT_LOAD FLAGS(4);
    data PT_LOAD FLAGS(6);
    tcb PT_LOAD FLAGS(6);
}

OUTPUT_FORMAT("elf32-i386")
ENTRY(start)
SECTIONS
{
  kphys_start = 0x00100000;
  kvirt_low_mem_start = 0xF8000000;
  kvirt_start = 0xF8100000;

  .boot kphys_start : AT(kphys_start) ALIGN(4K) {
    kvirt_disc = .;
    KEEP(*(.header))
      kdcode = .;
      *(.dtext)
      . = ALIGN(4);
      kddata = .;
      *(.ddata)
      . = ALIGN(4);
      *(.drodata)
      . = ALIGN(4K);
      kdend = .;
  } :boot

  /* Note that this is the address of the bottom of the stack. */

  .text kvirt_start + SIZEOF(.boot) : AT(kphys_start + SIZEOF(.boot)) ALIGN(4K) {
     kcode = .;
     *(.text*)
     . = ALIGN(4K);
     kcode_end = .;
  } :text

  .rodata : ALIGN(4K) {
    *(.rodata*)
    . = ALIGN(4);
    *(.debug*)
    . = ALIGN(4K);
  } :rodata

  .data : ALIGN(4K) {
     kdata = .;
     ksize = .;
     LONG(SIZEOF(.text) + SIZEOF(.rodata) + SIZEOF(.data) + SIZEOF(.bss));
     ktcb_table_size = .;
     LONG(SIZEOF(.tcb));

     *(.data*)
     . = ALIGN(4K);
     *(.tss)
     . = ALIGN(4K);
     kdata_end = .;
  } :data

  .bss : ALIGN(4K) {
    kbss = .;
    *(.bss*)
    *(.COMMON)
     . = ALIGN(4K);
    kbss_end = .;
  }

  .tcb : ALIGN(4M) {
    ktcb_start = .;
    tcb_table = .;
    *(.tcb)
     . = ALIGN(4K);
    ktcb_end = .;
  } :tcb

  kend = .;

  /DISCARD/ : {
    *(.comment*)
    *(.note*)
  }
}
