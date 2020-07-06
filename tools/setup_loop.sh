#!/bin/bash

FLOPPY_LOOP=/dev/loop2
RAM_LOOP=/dev/loop3
HD_LOOP=/dev/loop4

losetup $FLOPPY_LOOP ~/os_floppy.img
losetup $RAM_LOOP ~/rdiskimg.img
# 1st partition starts at 16384 bytes into file (start of 2nd track)
losetup -o 16384 $HD_LOOP ~/diskdrive_100-64-32.img
