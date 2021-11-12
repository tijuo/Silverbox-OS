OUTPUT_FORMAT("elf32-i386")
ENTRY(start)
SECTIONS
{
  kPhysCode  = 0x00100000;
  kPhysStart = 0x00100000;
  kVirtCode  = 0x80100000;
  kVirtStart = 0x80100000;

  kVirtToPhys = kPhysStart + ~kVirtStart + 1;
  kPhysToVirt = kVirtStart - kPhysStart;
  VPhysMemStart = 0x00000000;

  /* Note that this is the address of the bottom of the stack. */

  .text kVirtCode : AT(kPhysCode) ALIGN(4096) {
     kCode = .;
     *(.header)
     *(.text*)
     *(.rodata*)
     *(.comment)
  }

  .data : ALIGN(4096) {
     kPhysData = . + kVirtToPhys;
     kVirtData = .;
     kData = .;
     *(.data*)
     . = ALIGN(4096);
     *(.tss)
  }

  .boot : ALIGN(4096) {
    kPhysDisc = . + kVirtToPhys;
    kVirtDisc = .;
    kdCode = .;
    *(.dtext)
    . = ALIGN(4);
    kdData = .;
    *(.ddata)
  }
  
  .bss : ALIGN(4096) {
    kPhysBss = . + kVirtToPhys;
    kVirtBss = .;
    kBss = .;
    *(.bss)
    *(.COMMON)
    . = ALIGN(4);
    /*
    kernelStacksBottom = .;
    *(.stacks)
    kernelStacksTop = .;
    */
    kVirtPageStack = .;
  }
  
  .tcb : ALIGN(0x400000) {
    kTcbStart = .;
    tcbTable = .;
    *(.tcb)
    kTcbEnd = .;
  }
  
  kEnd = .;
  kTcbTableSize = kTcbEnd - kTcbStart;
  
  kSize = kEnd - kCode;
}
