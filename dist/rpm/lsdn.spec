#
# spec file for package lsdn
#
# Copyright (c) 2018 SUSE LINUX GmbH, Nuernberg, Germany.
#
# All modifications and additions to the file contributed by third parties
# remain the property of their copyright owners, unless otherwise agreed
# upon. The license for this file, and modifications and additions to the
# file, is the same license as for the pristine package itself (unless the
# license for the pristine package is not an Open Source License, in which
# case the license is the MIT License). An "Open Source License" is a
# license that conforms to the Open Source Definition (Version 1.9)
# published by the Open Source Initiative.

# Please submit bugfixes or comments via http://bugs.opensuse.org/
#

# fedora Tcl macros because SUSE's are not compatible
%{!?tcl_version: %global tcl_version %(echo 'puts $tcl_version' | tclsh)}
%{?tcl_archdir: %global tcl_sitearch %{tcl_archdir}}
%{!?tcl_sitearch: %global tcl_sitearch %{_libdir}/tcl%{tcl_version}}

Name:           lsdn
Version:        0.1
Release:        0
Summary:        Linux Software Defined Networking
License:        GPL-2.0
Group:          System/Base
Url:            https://github.com/asch/lsdn
Source0:        %{name}-%{version}.tar.xz
BuildRoot:      %{_tmppath}/%{name}-%{version}-build
BuildRequires:  cmake
BuildRequires:  linux-glibc-devel >= 4.14
BuildRequires:  pkgconfig(libdaemon)
BuildRequires:  pkgconfig(json-c)
BuildRequires:  pkgconfig(libmnl)
BuildRequires:  pkg-config
BuildRequires:  tcl-devel >= 8.6
BuildRequires:  uthash-devel

%description
LSDN is a library and toolkit to configure multi-tenant virtual networks
with tenant isolation using TC rules.

%prep
%setup -q

%build
%if ! 0%{?suse_version}
mkdir -p build
cd build
%endif
%cmake \
    -DCMAKE_SHARED_LINKER_FLAGS="-Wl,--as-needed -Wl,-z,now" \
    ..
make %{?_smp_mflags}

%install
cd build
%make_install
mkdir -p %{buildroot}%{tcl_sitearch}/lsdn
mv %{buildroot}%{_libdir}/lsdn/pkgIndex.tcl %{buildroot}%{tcl_sitearch}/lsdn

%files
%defattr(-,root,root,-)
%{_bindir}/lsctl
%{_bindir}/lsctlc
%{_bindir}/lsctld
%{_libdir}/liblsdn.*
%{_libdir}/liblsctl-tclext.so
%{_libdir}/pkgconfig/lsdn.pc
%{_includedir}/lsdn
%{tcl_sitearch}/lsdn

%changelog
