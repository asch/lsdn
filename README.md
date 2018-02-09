# LSDN - Linux Software Defined Networking

[![Build Status](https://travis-ci.org/asch/lsdn.svg?branch=master)](https://travis-ci.org/asch/lsdn)
[![Build Status](https://readthedocs.org/projects/lsdn/badge/?version=latest)](https://lsdn.readthedocs.io)

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

## Documentation

The complete documentation, including more detailed build and
instalation instructions is available at https://lsdn.readthedocs.io .

## Description

LSDN is a C library using TC (Traffic Control) Linux subsystem for comfortable management of virtual
networks. It is suitable for home/small office networks as well as for huge virtual networks in data
centers.

## Contact
We can be reached on mailing list <sp@asch.cz>.
