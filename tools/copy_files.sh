#!/bin/bash

# This copies executables into the appropriate directory on the drive
# Assumes that SB_MNT_PT is an existing mount point for the drive image

target_dir=
file_list=

usage ()
{
  echo "Usage: $0 -d <dir> <file1> [file2] ... [fileN]"
  printf "\t-d\tSets the target directory\n"
}

source `dirname $0`/set_variables.sh

while [ "$1" != "" ]; do
  if [ "$1" = "-d" ]; then
    shift
    target_dir="$1"
    shift
  else
    file_list="$file_list $1"
    shift
  fi
done

if [ -z "$target_dir" ] || [ -z "$file_list" ]; then
  usage
  exit 1
fi

# If already mounted, do not mount/unmount

if [ `cat /etc/mtab | grep "$SB_MNT_PT" > /dev/null; echo $?` -eq 1 ]; then
  mount $SB_MNT_PT;
fi

cp $file_list $SB_MNT_PT/$target_dir

sleep 0.5
umount $SB_MNT_PT

