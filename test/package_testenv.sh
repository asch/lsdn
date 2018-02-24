#!/bin/bash

./create_kernel.sh "$1"

cp "$1"/arch/x86/boot/bzImage .
make -C "$1" headers_install INSTALL_HDR_PATH=`pwd`/headers
tar -cv -f testenv.tar headers bzImage
xz -v testenv.tar
sha256sum testenv.tar.xz
