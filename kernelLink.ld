OUTPUT_FORMAT("elf32-i386")
ENTRY(start)
SECTIONS
{
  kPhysCode  = 0x00400000;
  kPhysStart = 0x00400000;
  kVirtCode  = 0xF0000000;
  kVirtStart = 0xF0000000;

  kVirtToPhys = kPhysStart - kVirtStart;
  kPhysToVirt = kVirtStart - kPhysStart;
  VPhysMemStart = 0x00000000;

  /* Note that this is the address of the bottom of the stack. */

  .text kVirtCode : AT(kPhysCode) ALIGN(4096) {
     kCode = .;
     *(.header)
     *(.text*)
     *(.rodata*)
     *(.comment)
     . = ALIGN(4096);
  }
  
  kPhysData = . + kVirtToPhys;
  kVirtData = .;

  .data : ALIGN(4096) {
     kData = .;
     *(.data*)
     . = ALIGN(4096);
  }

  kPhysDisc = . + kVirtToPhys;
  kVirtDisc = .;

  .disc : ALIGN(4096) {
    kdCode = .;
    *(.dtext)
    kdData = .;
    *(.ddata)
    . = ALIGN(4096);
    *(.dpages)
    . = ALIGN(4096);
  }

  kPhysBss = . + kVirtToPhys;
  kVirtBss = .;

  .bss : ALIGN(4096) {
    kBss = .;
    *(.pages)
    *(.bss)
    *(.COMMON)
    . = ALIGN(4096);
  }

  kEnd = ALIGN(4096);
  kSize = kEnd - kCode;
}
