#!/usr/bin/env bash

cp qemu/example.config ${KERNEL_PATH}/.config
make -C olddefconfig
make -C ${KERNEL_PATH} -j 4
# bzImage in ${KERNEL_PATH}/arch/x86/boot/bzImage
