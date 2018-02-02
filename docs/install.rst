============
Installation
============

.. highlight:: bash

-------------------
System requirements
-------------------

Following libraries are needed for compiling LSDN:

 - tcl
 - uthash
 - libmnl
 - json-c
 - libdaemon
 - libvirt (optional, only for libvirt demos)

You will also need CMake and (naturally) GCC for building the packages.

If you are running Ubuntu or Debain, run: ::

    apt install uthash-dev libmnl-dev libjson0-dev libdaemon-dev libvirt-dev cmake build-essential

If you are running Arch Linux, run: ::

    pacman -S tcl uthash libmnl json-c libdaemon libvirt cmake

You will also need fairly recent Linux Kernel headers (at least 4.14) to build
LSDN. For acutally running LSDN, we recommand 4.15, as 4.14 still has some bugs
in the used networking technologies and you might encounter crashes. This means
you will either need to run a recent version of your distro or install the
kernel manually.

If you do not plan on running LSDN on this machine, it is also possible to
install just the kernel headers.  by running: ::

    make headers_install INSTALL_HDR_PATH=$header_dir

--------------------
Building from source
--------------------

Simply install all the required software above and run these commands in the
directory where you put the downloaded sources: ::

    mkdir build
    cd build
    cmake ..
    make
    sudo make install

Now try running ``lsctl`` to see if the package was installed correctly.

If you have additional kernel headers installed manually (see previous section), instead of running
``cmake``, run: ::

    cmake -DKERNEL_HEADERS=$header_dir ..

------------------
Building packages
------------------

This project contains instruction files for building packages for various distributions of Linux.

On Arch Linux the PKGBUILD file is located in dist/arch/ and the package can be build and installed
as follows: ::

	cd dist/arch/
	makepkg
	pacman -U lsdn*.tar.xz

-------------
Running tests
-------------