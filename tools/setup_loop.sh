#!/bin/bash

FLOPPY_LOOP=/dev/loop2
RAM_LOOP=/dev/loop3

losetup $FLOPPY_LOOP /home/tiju/os_floppy.img
losetup $RAM_LOOP /home/tiju/rdiskimg.img
