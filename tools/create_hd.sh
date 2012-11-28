#!/bin/bash

size=

prefix ()
{
  prefixes=( '' Ki Mi Gi Ti )
  i=0
  val=$1

  while [ $i -le 4 ]; do
    if [ `printf "%.0f" $val` -ge 1024 -a $i -lt 4 ]; then
      val=`echo "$val / 1024.0" | bc -l`
      i=$((i+1))
      continue
    else
      size=`printf "%.2f %sB" $val ${prefixes[$i]}`
      return 0
    fi
  done
  return 1

#  if [ $1 -ge 1024 ]; then
#    if [ $1 -ge $((1024*1024)) ]; then
#      if [ $1 -ge $((1024*1024*1024)) ]; then
#        if [ $1 -ge $((1024*1024*1024*1024)) ]; then
#          size="$(($1/1024/1024/1024/1024)) TiB"
#        else
#          size="$(($1/1024/1024/1024)) GiB"
#          return 0
#        fi
#      else
#       size="$(($1/1024/1024)) MiB"
#       return 0
#      fi
#    else
#      size="$(($1/1024)) KiB"
#      return 0
#    fi
#  fi
#
#  size="$1 B"
}

until [ $cyls ] && [ $cyls -ge 1 ] && [ $cyls -le 65536 ]; do
  printf "Number of cylinders [1-65536]: "
  read cyls
done

until [ $heads ] && [ $heads -ge 1 ] && [ $heads -le 256 ]; do
  printf "Number of heads [1-256]: "
  read heads
done

until [ $secs ] && [ $secs -ge 1 ] && [ $secs -le 255 ]; do
  printf "Sectors per track [1-255]: "
  read secs
done

echo "Cylinders: $cyls  Heads: $heads  Sectors/track: $secs"
total=$(($cyls*$heads*$secs*512))
prefix $total
echo "Total sectors: $((total/512))"
echo "Size: $size"

printf "Create disk image [y/N]?: "
read create

if [ "$create" != "Y" ] && [ "$create" != "y" ]; then
  exit 1
fi

printf "Image name [disk_${cyls}-${heads}-${secs}.img]"
read img_name

if [ -z "$img_name" ]; then
  img_name="disk_${cyls}-${heads}-${secs}.img"
fi

if [ -f "$img_name" ]; then
  printf "$img_name already exists. Overwrite [y/N]? "
  read overwrite

  if [ "$overwrite" != "Y" ] && [ "$overwrite" != "y" ]; then
    exit 1
  fi
fi

dd if=/dev/zero of=$img_name bs=512 count=$(($total/512)) 2> /dev/null

if [ $? -ne 0 ]; then
  echo "Image creation failed."
  exit 1
else
  echo "Image created successfully."
fi
