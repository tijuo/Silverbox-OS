#!/bin/bash

# Assembles and links an assembly file into an executable

if [ $# -neq 1 ]; then
  echo "Usage: $0 <src-file>";
  exit 1;
fi

base=`basename $1 .asm`
nasm -f elf $1

if [ $? -eq 0 ]; then
  ld -T link.ld ${base}.o -o ${base}.exe;

  if [ $? -eq 0 ]; then
    echo Done;
  else
    exit 1;
  fi
else
   exit 1;
fi
