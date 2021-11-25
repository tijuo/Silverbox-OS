OUTPUT_FORMAT("elf32-i386")
ENTRY(start)
SECTIONS
{
  kPhysStart = 0x00100000;
  kVirtLowMemStart = 0xF8000000;
  kVirtStart = 0xF8100000;

  .boot kPhysStart : AT(kPhysStart) ALIGN(4K) {
    KEEP(*(.header))
    kVirtDisc = .;
    kdCode = .;
    *(.dtext)
    . = ALIGN(4);
    kdData = .;
    *(.ddata)
    . = ALIGN(4K);
    kdEnd = .;
  }

  /* Note that this is the address of the bottom of the stack. */

  .text kVirtStart + SIZEOF(.boot) : AT(kPhysStart + SIZEOF(.boot)) ALIGN(4K) {
     kCode = .;
     *(.text*)
     . = ALIGN(4);
     *(.rodata*)
     . = ALIGN(4);
     *(.debug*)
     . = ALIGN(4K);
     kCodeEnd = .;
  }

  .data : ALIGN(4K) {
     kData = .;

     kSize = .;
     LONG(SIZEOF(.text) + SIZEOF(.data) + SIZEOF(.bss));
     kTcbTableSize = .;
     LONG(SIZEOF(.tcb));

     *(.data*)
     . = ALIGN(4K);
     *(.tss)
     . = ALIGN(4K);
     kDataEnd = .;
  }
  
  .bss : ALIGN(4K) {
    kBss = .;
    *(.bss*)
    *(.COMMON)
     . = ALIGN(4K);
    kBssEnd = .;
  }
  
  .tcb : ALIGN(4M) {
    kTcbStart = .;
    tcbTable = .;
    *(.tcb)
     . = ALIGN(4K);
    kTcbEnd = .;
  }
  
  kEnd = .;

  /DISCARD/ : {
    *(.comment*)
    *(.note*)
  }
}
