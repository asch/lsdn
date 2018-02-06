# LSDN - Linux Software Defined Networking

[![Build Status](https://travis-ci.org/asch/lsdn.svg?branch=master)](https://travis-ci.org/asch/lsdn)

## Build
```
mkdir build
cd build
cmake ..
make
sudo make install
```

However chances are, you don't have a recent kernel. In that case, get new
kernel sources and install new kernel headers
`make headers_install INSTALL_HDR_PATH=$header_dir`. Instead of running `cmake
..` run `cmake -DKERNEL_HEADERS=$header_dir/include ..` and LSDN should build.

## Running tests
Running `sudo make test` might simply work for you, if you have a recent kernel (4.14) and you are
prepared to stress you kernel a bit. Also TCL >= 8.6.0 is needed.

However, as an alternative, we provide helper scripts to run LSDN tests under
QEMU. This way you can avoid being root, stressing you kernel or installing a
new kernel. Run `tests/run-qemu --help` in your build directory, it will tell
you what to prepare.

## Description

LSDN is a C library using TC (Traffic Control) Linux subsystem for comfortable management of virtual
networks. It is suitable for home/small office networks as well as for huge virtual networks in data
centers.

## Contact
We can be reached on mailing list <sp@asch.cz> and IRC channel `#lsdn` on Freenode.
