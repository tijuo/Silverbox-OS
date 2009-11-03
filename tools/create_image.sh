#!/bin/bash

# This will create a floppy disk image, but it must go through a GRUB install
# before it can be bootable

source `dirname $0`/variables.sh

LOOP_DEV="/dev/loop5"
OUTPUT="newimg.img"
UNMOUNT=0

if [ `whoami` != "root" ]; then
  echo "You must be root.";
  exit 1;
elif [ `which mkdosfs` == "" ]; then
  echo "mkdosfs is needed.";
  exit 1;
fi

if [ $# -eq 1 ]; then
  OUTPUT=$1;
fi

dd if=/dev/zero of=$OUTPUT bs=512 count=2880 
mkdosfs -f 2 -F 12 -h 0 -r 112 -R 1 -s 2 -S 512 $OUTPUT

if [ `cat /etc/mtab | grep "$SB_MNT_PT" > /dev/null; echo $?` -eq 1 ]; then
  mount $OUTPUT ./mnt -t msdos -o loop,blocksize=512;
  UNMOUNT=1;
fi

mkdir ./mnt/{boot,$SB_DISK_DIR} ./mnt/boot/grub
cp stage{1,2} menu.lst ./mnt/boot/grub &&

if [ $UNMOUNT -eq 1 ]; then
  umount ./mnt;
fi
