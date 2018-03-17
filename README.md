# LSDN - Linux Software Defined Networking

[![Build Status](https://travis-ci.org/asch/lsdn.svg?branch=master)](https://travis-ci.org/asch/lsdn)
[![Build Status](https://readthedocs.org/projects/lsdn/badge/?version=latest)](https://lsdn.readthedocs.io)

## Description

LSDN is a C library using TC (Traffic Control) Linux subsystem for comfortable
management of virtual networks. It is suitable for home/small office networks as
well as for huge virtual networks in data centers.

## Installation 

### Arch Linux

There is a package in the [AUR](https://aur.archlinux.org/) called
[lsdn-git](https://aur.archlinux.org/packages/lsdn-git/).

### RPM based distributions

Fedora and openSUSE packages are available from the Open Build Service project
[home:matejcik:lsdn](https://build.opensuse.org/project/show/home:matejcik:lsdn).

RPMs themselves can be found here: https://download.opensuse.org/repositories/home:/matejcik:/lsdn/

### Manual Build
```
mkdir build
cd build
cmake ..
make
sudo make install
```
However chances are, you don't have a recent kernel. In that case, get new
kernel sources and install new kernel headers `make headers_install
INSTALL_HDR_PATH=$header_dir`. Instead of running `cmake ..` run `cmake
-DKERNEL_HEADERS=$header_dir/include ..` and LSDN should build.

## Quick-Start

See [Quick-Start Section](http://lsdn.readthedocs.io/en/latest/quickstart.html)
for kick-start.

## Documentation

The complete documentation, including more detailed build and installation
instructions is available at https://lsdn.readthedocs.io .

## Contribution

Feel free making a pull request with your patches. Please stick to [Linux kernel
coding style](https://www.kernel.org/doc/html/v4.10/process/coding-style.html)
with **one exception** --- the limit on the length of lines is 100 columns and
not 80 as in the Linux kernel.

## Contact
We can be reached on mailing list <sp@asch.cz>.
