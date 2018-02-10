#!/bin/bash

VERSION="0.1"

# find path to source root
_me=`realpath "$0"`
_dir=`dirname "$_me"`
SOURCE_ROOT=`realpath "$_dir/../.."`

pushd "$_dir"

# prepare tarball
TMPDIR=`mktemp -d`
# tar will not like the leading slash in .gitignore
sed 's%^/%%' "$SOURCE_ROOT"/.gitignore > $TMPDIR/ignorefile
tar -cvJ -f $TMPDIR/lsdn-${VERSION}.tar.xz \
    --exclude-from $TMPDIR/ignorefile --exclude=".git" \
    -C "$SOURCE_ROOT" \
    --transform "s%^\\.%lsdn-${VERSION}%" \
    .
mv $TMPDIR/lsdn-${VERSION}.tar.xz .
rm -r $TMPDIR

# prepare rpm topdir
mkdir -p rpmbuild/{BUILD,BUILDROOT,RPMS,SRPMS}
rpmbuild -D "_topdir $PWD/rpmbuild" -D "_sourcedir $PWD" -ba lsdn.spec \
    $@ \
    || exit 1

# collect built RPMs
cp rpmbuild/RPMS/*/*.rpm rpmbuild/SRPMS/*.rpm .
rm -r rpmbuild
