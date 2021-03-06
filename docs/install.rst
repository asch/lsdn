============
Installation
============

.. highlight:: bash

-------------------
System requirements
-------------------

The following libraries are needed for compiling LSDN:

 - tcl >= 8.6.0
 - uthash
 - libmnl
 - json-c
 - libdaemon
 - linux-headers >= 4.14
 - libvirt (optional, only for libvirt demos)

You will also need CMake and (naturally) GCC for building the packages.

If you are running **Ubuntu** or **Debain**, run: ::

    apt install tcl-dev uthash-dev libmnl-dev libjson-c-dev libdaemon-dev libvirt-dev cmake build-essential

If you are running **Arch Linux**, run: ::

    pacman -S tcl uthash libmnl json-c libdaemon libvirt cmake

If you are running **CentOS 7**, run: ::

    yum install gcc cmake uthash-devel libmnl-devel libdaemon-devel json-c-devel

However, you will need the ``EPEL`` repository for the ``uthash`` package and
you will need to install ``tcl-devel`` package from a different source (for
example
`Psychotic Ninja <https://centos.pkgs.org/7/psychotic-ninja-plus-x86_64/tcl-devel-8.6.5-2.el7.psychotic.x86_64.rpm.html>`_).

If you are running **openSUSE**, run: ::

    zypper install gcc cmake uthash libmnl-devel libdaemon-devel linux-glibc-devel tcl-devel libjson-c-devel

You will also need fairly recent Linux Kernel headers (at least 4.14) to build
LSDN. To actually run LSDN, we recommend 4.15, as 4.14 still has some bugs in
the used networking technologies and you might encounter crashes. This means you
will either need to run a recent version of your distribution or install the
kernel manually.

The exception is RHEL and CentOS -- they backport features into their kernels
very agressively and you migh be get lucky even with an 3.x CentOS/RHEL kernel.

If you do not plan on running LSDN on your machine, it is also possible to
install just the kernel headers by running: ::

    make headers_install INSTALL_HDR_PATH=$header_dir

--------------------
Building from source
--------------------

Simply install all the required software listed above and run these commands in
the directory where you put the downloaded sources: ::

    mkdir build
    cd build
    cmake ..
    make
    sudo make install

Now try running ``lsctl`` to see if the package was installed correctly.

If ``lsctl`` does not run correctly, check if your distribution looks for
libraries in ``/usr/local/lib`` or ``/usr/local/lib64``. Some distributions do
not look for libraries in user-installed paths by default. To check the
effective linker path, use ``ldconfig -v 2>/dev/null | grep -v ^$'\t'``.

If you have installed kernel headers manually (see previous section), instead
of running ``cmake ..``, run: ::

    cmake -DKERNEL_HEADERS=$header_dir/include ..

------------------
Building packages
------------------

This project contains instruction files for building packages for various
distributions of Linux.

Arch
~~~~

The PKGBUILD file for Arch Linux is located in ``dist/arch/`` and the package can
be built and installed as follows: ::

	cd dist/arch/
	makepkg
	pacman -U lsdn*.tar.xz

If you do not want to build the package on your own, you can install lsdn with
all it's dependencies directly from Arch User Repository (lsdn-git package): ::

	pacaur -S lsdn-git # pacaur is AUR helper of our choice

Debian
~~~~~~

See https://wiki.archlinux.org/index.php/installation_guide :)

Jokes aside, run: ::

    ln -s dist/debian .
    dpkg-buildpackage

The packages ``lsdn`` and ``lsdn-dev`` will be available in the parent folder.

RPM-based distributions
~~~~~~~~~~~~~~~~~~~~~~~

Spec file is located in ``dist/rpm`` directory. In addition, a convenience
build script is provided. Make sure your system has all the dependencies (see
above) and also ``rpm-build``. Then, run: ::

    sh dist/rpm/rpmbuild.sh

Resulting rpms will be placed in the ``dist/rpm`` directory.

-------------
Running tests
-------------

LSCTL includes a test-suite that constructs various virtual networks and tries
pinging VMs inside those networks. ``sudo make test`` starts these tests.

If you plan on developing LSDN, you might want to run the tests inside another
level of VM. There is already a testing environment ready for those purposes,
built on QEMU and minimal Arch root file system. More information can be found
in the Developer documentation section :ref:`test_harness`.
