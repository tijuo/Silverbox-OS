PHDRS {
    boot PT_LOAD FLAGS(7);
    text PT_LOAD FLAGS(5);
    rodata PT_LOAD FLAGS(4);
    data PT_LOAD FLAGS(6);
}

OUTPUT_FORMAT("elf32-i386")
ENTRY(_start)
SECTIONS
{
    kphys_start = 0x00100000;
    kvirt_low_mem_start = 0xFF800000;
    kvirt_start = 0xFEC00000;

    .boot kvirt_start : AT(kphys_start) ALIGN(4K) {
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

    .text : ALIGN(4K) {
       kcode = .;
       *(.text*)
       kcode_end = .;
    } :text

    .rodata : ALIGN(4K) {
      krodata = .;
      *(.rodata*)
      . = ALIGN(4);
      *(.debug*)
      krodata_end = .;
    } :rodata

    .data : ALIGN(4K) {
        kdata = .;
        ksize = .;
        LONG(SIZEOF(.boot) + SIZEOF(.text) + SIZEOF(.rodata) + SIZEOF(.data) + SIZEOF(.bss));

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

    kend = .;

    /DISCARD/ : {
      *(.comment*)
      *(.note*)
    }
}
