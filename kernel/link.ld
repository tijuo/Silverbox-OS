OUTPUT_FORMAT("elf64-x86-64")
ENTRY(start)
SECTIONS
{
  kphys_start = 0x100000;
  kvirt_low_mem_start = 0;
  kvirt_start = 0x100000;

  .boot kphys_start : AT(kphys_start) ALIGN(4K) {
    KEEP(*(.header))
    kvirt_disc = .;
    kdcode = .;
    *(.dtext)
    kddata = .;
    *(.ddata)
    . = ALIGN(4K);
    kdend = .;
  }

  .text kvirt_start + SIZEOF(.boot) : AT(kphys_start + SIZEOF(.boot)) ALIGN(4K) {
     kcode = .;
     *(.text*)
     *(.rodata*)
     *(.debug*)
     *(.interp*)
     *(.rela*)
     . = ALIGN(4K);
     kcode_end = .;
  }

  .data : ALIGN(4K) {
     kdata = .;

     ksize = .;
     QUAD(SIZEOF(.text) + SIZEOF(.data) + SIZEOF(.bss));
     ktcb_table_size = .;
     QUAD(SIZEOF(.tcb));

     *(.data*)
     . = ALIGN(4K);
     *(.dynamic*)
     *(.dynstr*)
     *(.dynsym*)
     *(.got*)
     *(.rel*)
     *(.shstrtab*)
     *(.strtab*)
     *(.symtab*)
     *(.hash*)
     *(.tss)
     . = ALIGN(4K);
     kdata_end = .;
  }

  .bss : ALIGN(4K) {
    kbss = .;
    *(.bss*)
    *(.COMMON)
     . = ALIGN(4K);
    kbss_end = .;
  }

  .tcb : ALIGN(2M) {
    ktcb_start = .;
    tcb_table = .;
    *(.tcb)
     . = ALIGN(4K);
    ktcb_end = .;
  }

  kend = .;

  /DISCARD/ : {
    *(.comment*)
    *(.note*)
  }
}
