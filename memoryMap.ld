  kPhysCode  = 0x1000000;
  kPhysStart = 0x1000000;
  kVirtCode  = 0x80000000;
  kVirtStart = 0x80000000;

  kVirtToPhys = kPhysStart - kVirtStart;
  kPhysToVirt = kVirtStart - kPhysStart;
  VPhysMemStart = 0x00000000;

  _kernelIDT = VPhysMemStart + 0x88000;
  _kernelGDT = 0x88800 + VPhysMemStart;
  _kernelTSS = 0x88F98 + VPhysMemStart;
  _initKrnlPDIR = _kernelIDT + 0x1000;
  _firstKrnlPTAB = _initKrnlPDIR + 0x1000;
  _initServPDIR = _firstKrnlPTAB + 0x1000;
  _initServStackPTAB = _initServPDIR + 0x1000;

  /* Note that this is the address of the bottom of the stack. */

  _initServStack = _initServStackPTAB + 0x1000;

  _initServPTAB = _initServStack + 0x1000;

  /* Note that these addresses are the bottoms of the stacks. */

  _idleStack = _initServPTAB + 0x1000;
  _kernelStack = _idleStack + 0x1000;
  _irqStack = _kernelStack + 0x2000;

  _kernelVars = _irqStack + 0x1000;
  _firstPTAB = _kernelVars + 0x1000;
  _secondKrnlPTAB = _firstPTAB + 0x1000;

  /* Note that this is the address of the bottom of the stack. */

  _kernelBootStack = _secondKrnlPTAB + 0x1000;
