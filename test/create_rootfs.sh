#!/usr/bin/env bash

rm -rf rootfs
mkdir -p rootfs/var/lib/pacman
fakeroot -- pacman --noconfirm -r rootfs -Syy base cmake valgrind gdb tcl dhcp tcpdump
chmod -s rootfs/bin/mount
cp qemu/lsdn-guest-init rootfs/