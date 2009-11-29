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
IMPORT kernelGDT
IMPORT kernelIDT

kCodeSel  equ 0x08
kDataSel  equ 0x10
uCodeSel  equ 0x1B
uDataSel  equ 0x23
tssSel    equ 0x28

%endif
