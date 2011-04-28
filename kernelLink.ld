OUTPUT_FORMAT("elf32-i386")
ENTRY(start)
SECTIONS
{
  kPhysCode  = 0x1000000;
  kPhysStart = 0x1000000;
  kVirtCode  = 0xFF400000;
  kVirtStart = 0xFF400000;

  kVirtToPhys = kPhysStart - kVirtStart;
  kPhysToVirt = kVirtStart - kPhysStart;
  VPhysMemStart = 0x00000000;

  _kernelIDT = VPhysMemStart + 0x88000;
  _kernelGDT = 0x88800 + VPhysMemStart;
  _kernelTSS = 0x88F98 + VPhysMemStart;
  _kernelIOBitmap = _kernelIDT + 0x1000;
  _initKrnlPDIR = _kernelIOBitmap + 0x2000;
  _firstKrnlPTAB = _initKrnlPDIR + 0x1000;
  _initServPDIR = _firstKrnlPTAB + 0x1000;
  _initServStackPTAB = _initServPDIR + 0x1000;

  /* Note that this is the address of the bottom of the stack. */

  _initServStack = _initServStackPTAB + 0x1000;

  _initServPTAB = _initServStack + 0x1000;

  /* Note that these addresses are the bottoms of the stacks. */

  _idleStack = _initServPTAB + 0x1000;
  _kernelStack = _idleStack + 0x1000;

  _kernelVars = _kernelStack + 0x1000;
  _firstPTAB = _kernelVars + 0x1000;
  _secondKrnlPTAB = _firstPTAB + 0x1000;
  _initFirstPTAB = _secondKrnlPTAB + 0x1000;

  /* Note that this is the address of the bottom of the stack. */

  _kernelBootStack = _initFirstPTAB + 0x1000;

  .text kVirtCode : AT(kPhysCode) {
     kCode = .;
     *(.header)
     *(.text*)
     *(.rodata*)
     *(.comment)
     . = ALIGN(4096);
  }
  
  kPhysData = . + kVirtToPhys;
  kVirtData = .;

  .data . : AT(kPhysData) {
     kData = .;
     *(.data*)
     . = ALIGN(4096);
  }

  kPhysDisc = . + kVirtToPhys;
  kVirtDisc = .;

  .disc . : AT(kPhysDisc) {
    kdCode = .;
    *(.dtext)
    kdData = .;
    *(.ddata)
    . = ALIGN(4096);
  }

  kPhysBss = . + kVirtToPhys;
  kVirtBss = .;

  .bss kVirtBss : AT(kPhysBss) {
     kBss = .;
     *(.bss)
/*     . = ALIGN(4096); */
  }
  
  kEnd = .;
  kSize = kEnd - kCode;
}
