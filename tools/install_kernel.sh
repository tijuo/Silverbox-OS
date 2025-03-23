#!/bin/sh

TFTP_ROOT=/home/user/tftp
rel_path=/

if [ $# -eq 0 ]; then
    echo "No files have been specified.";
    exit 1;
fi

while [ $# -gt 0 ]; do
    if [ $1 = "-d" ]; then
        rel_path=$2;
        shift;
    else
        cp "$1" "$TFTP_ROOT$rel_path"
    fi
    shift
done
