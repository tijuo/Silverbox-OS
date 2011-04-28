%ifndef KERNEL_INC
%define KERNEL_INC

%include "asm/asm.inc"

[section .data]
IMPORT kCode
IMPORT kData
IMPORT kBss
IMPORT kEnd
IMPORT kdCode
IMPORT kdData
IMPORT kPhysStart
IMPORT kVirtStart
IMPORT kPhysData
IMPORT kVirtData
IMPORT kPhysBss
IMPORT kVirtBss
IMPORT kSize
IMPORT kPhysToVirt
IMPORT kVirtToPhys
IMPORT VPhysMemStart
IMPORT _kernelGDT
IMPORT _kernelIDT
IMPORT _kernelTSS
IMPORT _initKrnlPDIR
IMPORT _firstKrnlPTAB
IMPORT _initServPDIR
IMPORT _initServStackPTAB
IMPORT _initServStack
IMPORT _initServPTAB
IMPORT _idleStack
IMPORT _kernelStack
IMPORT _kernelVars
IMPORT _firstPTAB
IMPORT _secondKrnlPTAB
IMPORT _kernelBootStack
IMPORT _initFirstPTAB

kCodeSel  equ 0x08
kDataSel  equ 0x10
uCodeSel  equ 0x1B
uDataSel  equ 0x23
tssSel    equ 0x28

%endif
