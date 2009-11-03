; grub.inc : Include file for kernel_start.asm
;
; Copyright (C) 2006 Tiju Oliver
;
; This program is free software; you can redistribute it and/or modify it under
; the terms of the GNU General Public License as published by the Free Software
; Foundation; either version 2 of the License, or (at your option) any later
; version.
;
; This program is distributed in the hope that it will be useful, but WITHOUT
; ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
; FOR A PARTICULAR PURPOSE.
;
; For more info, look at the COPYING file.
;

MULTIBOOT_PAGE_ALIGN	equ 1 << 0
MULTIBOOT_MEMORY_INFO	equ 1 << 1
MULTIBOOT_AOUT_KLUDGE	equ 1 << 16
MULTIBOOT_HEADER_MAGIC	equ 0x1BADB002
MULTIBOOT_HEADER_FLAGS	equ MULTIBOOT_PAGE_ALIGN | MULTIBOOT_MEMORY_INFO | MULTIBOOT_AOUT_KLUDGE
MULTIBOOT_CHECKSUM	equ -(MULTIBOOT_HEADER_MAGIC + MULTIBOOT_HEADER_FLAGS)
