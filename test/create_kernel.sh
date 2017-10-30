#!/usr/bin/env bash

KERNEL_PATH=$1

if [ -z "$KERNEL_PATH" ]; then
	echo "Usage: $0 <KERNEL_PATH>"
	exit 1;
fi

cp qemu/example.config ${KERNEL_PATH}/.config
make -C ${KERNEL_PATH} olddefconfig
make -C ${KERNEL_PATH} -j 4
# bzImage in ${KERNEL_PATH}/arch/x86/boot/bzImage
