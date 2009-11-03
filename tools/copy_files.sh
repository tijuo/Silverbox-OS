#!/bin/bash

# This copies executables into the appropriate directory on the drive
# Assumes that SB_MNT_PT is an existing mount point for the drive image

source `dirname $0`/variables.sh

if [ $# -lt 1 ]; then
  echo "Usage: $0 <file1> [file2] ... [fileN]";
  exit 1;
fi

UNMOUNT=0

# If already mounted, do not mount/unmount

if [ `cat /etc/mtab | grep "$SB_MNT_PT" > /dev/null; echo $?` -eq 1 ]; then \
  mount $SB_MNT_PT; \
  UNMOUNT=1; \
fi

cp $* $SB_MNT_PT/$SB_DISK_DIR

if [ $UNMOUNT -eq 1 ]; then \
  umount $SB_MNT_PT; \
fi

