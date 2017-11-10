#!/usr/bin/env bash
rm -rf rootfs
mkdir -p rootfs/var/lib/pacman
mkdir -p pacman-cache
OPTIONAL="gdb tcpdump"
fakeroot -- pacman --cachedir $(pwd)/pacman-cache/ --noconfirm -r rootfs -Syy bash cmake dhcp util-linux iproute2 grep valgrind procps-ng iputils tcl dhcpcd
chmod -s rootfs/bin/mount
cp qemu/lsdn-guest-init rootfs/
