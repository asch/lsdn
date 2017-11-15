#!/usr/bin/env bash
rm -rf rootfs
mkdir -p rootfs/var/lib/pacman
mkdir -p pacman-cache
PACKAGES="bash cmake dhcp util-linux iproute2 grep valgrind procps-ng iputils tcl dhcpcd"
OPTIONAL="gdb tcpdump pacman binutils"
if [ "${MINIMAL:-0}" -ne "1" ]; then
    PACKAGES="$PACKAGES $OPTIONAL"
fi
fakeroot -- pacman --cachedir $(pwd)/pacman-cache/ --noconfirm -r rootfs -Syy $PACKAGES
chmod -s rootfs/bin/mount
cp qemu/lsdn-guest-init rootfs/
